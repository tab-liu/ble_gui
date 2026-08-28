#include "can_data.h"
#include "esp_log.h"
#include <stdint.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "crc.h"
#include "comm_define.h"
#include "uart_device_process.h"
#include "iot_period_task.h"

#include "can_protocol.h"
#include "udt_transfer.h"
#include "app_time.h"
#include "data_summary.h"

#include "iot_mqtt.h"
#include "iot_wifi_init.h"
#include "filesystem.h"
#include "app_param.h"
#include <string.h>

#define TAG "[CAN_DATA]"

/* 调试：打印 CAN 0x11(inv_base) 接收拼包完成、写入 Inv_can 之前的原始 buf 与字段解析。联调时改为 1，量产保持 0 */
#ifndef CAN_DEBUG_LOG_0X11_RX
#define CAN_DEBUG_LOG_0X11_RX 0
#endif

#if CAN_DEBUG_LOG_0X11_RX
static void can_debug_log_inv_base_0x11_before_copy(uint8_t dev_id, uint16_t write_off, const uint8_t *buf, uint16_t data_len)
{
	ESP_LOGI(TAG, "0x11 RX (before memcpy->Inv_can) devId=0x%02x write_off=%u data_len=%u",
		 dev_id, write_off, (unsigned)data_len);
	ESP_LOG_BUFFER_HEX(TAG, buf, data_len);

	if (write_off != 0U) {
		ESP_LOGW(TAG, "0x11 fragment write_off!=0, skip inv_base field decode");
		return;
	}

	inv_base_struct ib = {0};
	uint16_t copy_n = data_len;
	if (copy_n > (uint16_t)sizeof(ib)) {
		copy_n = (uint16_t)sizeof(ib);
	}
	memcpy(&ib, buf, copy_n);

	ESP_LOGI(TAG, "inv_base parsed (%u/%zu B): inv_num=%u inv_online=0x%04x inv_power_rang=%u line_event=0x%04x inv_work_state=%u ctrl_status.all=0x%04x",
		 (unsigned)copy_n, sizeof(inv_base_struct),
		 (unsigned)ib.inv_num, ib.inv_online, (unsigned)ib.inv_power_rang,
		 ib.line_event, (unsigned)ib.inv_work_state, ib.ctrl_status.all);
	ESP_LOGI(TAG, "  alarm: 0x%04x 0x%04x 0x%04x 0x%04x | fault: 0x%04x 0x%04x 0x%04x 0x%04x | fault5: 0x%04x",
		 ib.alarm[0], ib.alarm[1], ib.alarm[2], ib.alarm[3],
		 ib.fault[0], ib.fault[1], ib.fault[2], ib.fault[3], ib.fault5);
	ESP_LOGI(TAG, "  W: DCLoadAll=%lu ACLoadAll=%lu PVAll=%lu GridAll=%lu InvAll=%lu",
		 (unsigned long)ib.DCLoadAllTotalPower, (unsigned long)ib.ACLoadAllTotalPower,
		 (unsigned long)ib.PVAllTotalPower, (unsigned long)ib.GridAllTotalPower,
		 (unsigned long)ib.InvAllTotalPower);
	ESP_LOGI(TAG, "  Wh: DCLoadE=%lu ACLoadE=%lu PvChgE=%lu GridChgE=%lu FeedbackE=%lu",
		 (unsigned long)ib.DCLoadTotalEnergy, (unsigned long)ib.ACLoadTotalEnergy,
		 (unsigned long)ib.PvTotalChargingEnergy, (unsigned long)ib.GridTotalChargingEnergy,
		 (unsigned long)ib.FeedbackEnergy);
	ESP_LOGI(TAG, "  PvToACLoadE=%lu SelfCons%%=%u PVToACloadP=%lu grid_par_soc=0x%04x",
		 (unsigned long)ib.PvToACLoadEnergy, (unsigned)ib.SelfConsumptionPercent,
		 (unsigned long)ib.PVToACloadPower, ib.grid_par_soc);
	ESP_LOGI(TAG, "  switch_memory_state=%u", ib.switch_memory_state);
	ESP_LOGI(TAG, "  rw_cmd: off=%u rem=%u crc16=0x%04x ok=%u next_seq=%u dev=0x%02x type=0x%02x ptr=%p",
		 ib.rw_cmd.write_offset, ib.rw_cmd.write_remain_len, ib.rw_cmd.write_crc16,
		 (unsigned)ib.rw_cmd.crc_valid, ib.rw_cmd.write_next_seq, ib.rw_cmd.devId,
		 ib.rw_cmd.can_type, (void *)ib.rw_cmd.temp_buffer);
}
#endif /* CAN_DEBUG_LOG_0X11_RX */

/* 调试：CAN 0x16(inv_load) 拼包完成、memcpy 前原始 buf 与字段解析。量产改为 0 */
#ifndef CAN_DEBUG_LOG_0X16_RX
#define CAN_DEBUG_LOG_0X16_RX 0
#endif

#if CAN_DEBUG_LOG_0X16_RX
static void can_debug_log_inv_load_0x16_before_copy(uint8_t dev_id, uint16_t write_off, const uint8_t *buf, uint16_t data_len)
{
	ESP_LOGI(TAG, "0x16 RX (before memcpy->Inv_can) devId=0x%02x write_off=%u data_len=%u",
		 dev_id, write_off, (unsigned)data_len);
	ESP_LOG_BUFFER_HEX(TAG, buf, data_len);

	if (write_off != 0U) {
		ESP_LOGW(TAG, "0x16 fragment write_off!=0, skip inv_load field decode");
		return;
	}

	inv_load_struct il = {0};
	uint16_t copy_n = data_len;
	if (copy_n > (uint16_t)sizeof(il)) {
		copy_n = (uint16_t)sizeof(il);
	}
	memcpy(&il, buf, copy_n);

	ESP_LOGI(TAG, "inv_load parsed (%u/%zu B): total_dc_load_power=%lu total_dc_load_energy=%lu",
		 (unsigned)copy_n, sizeof(inv_load_struct),
		 (unsigned long)il.total_dc_load_power, (unsigned long)il.total_dc_load_energy);
	ESP_LOGI(TAG, "  DC 05V: P=%u I=%u | 12V: P=%u I=%u | 24V: P=%u I=%u",
		 il.dc_05v_load_power, il.dc_05v_load_current,
		 il.dc_12v_load_power, il.dc_12v_load_current,
		 il.dc_24v_load_power, il.dc_24v_load_current);
	ESP_LOGI(TAG, "  total_ac_load_power=%lu total_ac_load_energy=%lu ac_phase_number=%u",
		 (unsigned long)il.total_ac_load_power, (unsigned long)il.total_ac_load_energy,
		 (unsigned)il.ac_phase_number);
	for (int i = 0; i < 6; i++) {
		ESP_LOGI(TAG, "  ac_load[%d]: load_power=%u load_voltage=%u load_current=%u",
			 i,
			 (unsigned)il.ac_load[i].load_power,
			 (unsigned)il.ac_load[i].load_voltage,
			 (unsigned)il.ac_load[i].load_current);
	}
	ESP_LOGI(TAG, "  rw_cmd: off=%u rem=%u crc16=0x%04x ok=%u next_seq=%u dev=0x%02x type=0x%02x ptr=%p",
		 il.rw_cmd.write_offset, il.rw_cmd.write_remain_len, il.rw_cmd.write_crc16,
		 (unsigned)il.rw_cmd.crc_valid, il.rw_cmd.write_next_seq, il.rw_cmd.devId,
		 il.rw_cmd.can_type, (void *)il.rw_cmd.temp_buffer);
}
#endif /* CAN_DEBUG_LOG_0X16_RX */

/* 调试：CAN 0x15(inv_grid) 拼包完成、memcpy 前原始 buf 与字段解析。量产改为 0 */
#ifndef CAN_DEBUG_LOG_0X15_RX
#define CAN_DEBUG_LOG_0X15_RX 0
#endif

#if CAN_DEBUG_LOG_0X15_RX
static void can_debug_log_inv_grid_0x15_before_copy(uint8_t dev_id, uint16_t write_off, const uint8_t *buf, uint16_t data_len)
{
	ESP_LOGI(TAG, "0x15 RX (before memcpy->Inv_can) devId=0x%02x write_off=%u data_len=%u",
		 dev_id, write_off, (unsigned)data_len);
	ESP_LOG_BUFFER_HEX(TAG, buf, data_len);

	if (write_off != 0U) {
		ESP_LOGW(TAG, "0x15 fragment write_off!=0, skip inv_grid field decode");
		return;
	}

	inv_grid_struct ig = {0};
	uint16_t copy_n = data_len;
	if (copy_n > (uint16_t)sizeof(ig)) {
		copy_n = (uint16_t)sizeof(ig);
	}
	memcpy(&ig, buf, copy_n);

	ESP_LOGI(TAG, "inv_grid parsed (%u/%zu B): freq=%u total_chg_power=%lu total_chg_energy=%lu total_fb_energy=%lu grid_phase_number=%u",
		 (unsigned)copy_n, sizeof(inv_grid_struct),
		 ig.freq,
		 (unsigned long)ig.total_chg_power,
		 (unsigned long)ig.total_chg_energy,
		 (unsigned long)ig.total_fb_energy,
		 (unsigned)ig.grid_phase_number);
	for (int i = 0; i < 3; i++) {
		ESP_LOGI(TAG, "  grid_detail[%d]: input_power=%d input_voltage=%u input_current=%d",
			 i,
			 (int)ig.grid_detail[i].input_power,
			 (unsigned)ig.grid_detail[i].input_voltage,
			 (int)ig.grid_detail[i].input_current);
	}
	ESP_LOGI(TAG, "  Sign_Valid=%u grid_angle=%d", (unsigned)ig.Sign_Valid, (int)ig.grid_angle);
	ESP_LOGI(TAG, "  rw_cmd: off=%u rem=%u crc16=0x%04x ok=%u next_seq=%u dev=0x%02x type=0x%02x ptr=%p",
		 ig.rw_cmd.write_offset, ig.rw_cmd.write_remain_len, ig.rw_cmd.write_crc16,
		 (unsigned)ig.rw_cmd.crc_valid, ig.rw_cmd.write_next_seq, ig.rw_cmd.devId,
		 ig.rw_cmd.can_type, (void *)ig.rw_cmd.temp_buffer);
}
#endif /* CAN_DEBUG_LOG_0X15_RX */

EXT_RAM_BSS_ATTR device_data_struct  g_device_data;

static const data_abstract_struct  g_abstract_array_inv[]  = {   /* inv */
    {.internal = 1000, .addr = INV_CAN_ADDR, .type = 0x10, .max_len = sizeof(inv_announce_struct) - sizeof(rw_cmd_struct),},    // 逆变主包广播发送
    {.internal = 1000, .addr = INV_CAN_ADDR, .type = 0x11, .max_len = 0,    },    // 逆变汇总后数据
    {.internal = 1000, .addr = INV_CAN_ADDR, .type = 0x12, .max_len = 0,    },    // 能量线
    {.internal = 8000, .addr = INV_CAN_ADDR, .type = 0x13, .max_len = 0,   },    // 关于逆变,底层会根据该指令时间判断通信是否掉线
    {.internal = 2500, .addr = INV_CAN_ADDR, .type = 0x14, .max_len = 0,    },    // pv输入信息
    {.internal = 2500, .addr = INV_CAN_ADDR, .type = 0x15, .max_len = 0,    },    // 电网信息
    {.internal = 2500, .addr = INV_CAN_ADDR, .type = 0x16, .max_len = 0,    },    // 负载信息
    {.internal = 2500, .addr = INV_CAN_ADDR, .type = 0x17, .max_len = 0,    },    // 逆变信息
    {.internal = 2500, .addr = INV_CAN_ADDR, .type = 0x18, .max_len = 0,    },    // 电表信息
    {.internal = 3000, .addr = INV_CAN_ADDR, .type = 0x19, .max_len = 0,    },    // 发电机信息
    {.internal = 3000, .addr = INV_CAN_ADDR, .type = 0x1A, .max_len = 0,    },    // 设置数据00
    {.internal = 3000, .addr = INV_CAN_ADDR, .type = 0x1B, .max_len = 0,    },    // 设置数据01
    {.internal = 3000, .addr = INV_CAN_ADDR, .type = 0x1C, .max_len = 0,    },    // 设置数据02
    {.internal = 5000, .addr = INV_CAN_ADDR, .type = 0x1D, .max_len = 0,    },    // 设置数据03
    {.internal = 2000, .addr = INV_CAN_ADDR, .type = 0x20, .max_len = 0,    },    // 历史记录
    {.internal = 2000, .addr = INV_CAN_ADDR, .type = 0x21, .max_len = 0,    },    // 往年发电量信息
    {.internal = 2000, .addr = INV_CAN_ADDR, .type = 0x22, .max_len = 0,    },    // 今年发电量信息
    {.internal = 10000,.addr = INV_CAN_ADDR, .type = 0x23, .max_len = 0,    },    // 第三方wifi信息
    {.internal = 3500, .addr = INV_CAN_ADDR, .type = 0x24, .max_len = 0,    },    // 上报给外置wifi的数据
    {.internal = 4500, .addr = INV_CAN_ADDR, .type = 0x25, .max_len = 0,    },    // 外置wifi下发的数据(.max_len为0表示读取该类型的所有数据)
};

static const data_abstract_struct  g_abstract_array_pack[] = {   /* pack */
    {.internal = 1500, .addr = PACK_CAN_ADDR, .type = 0x50,  .max_len = sizeof(pack_announce_struct) - sizeof(rw_cmd_struct),   },    // pack主包广播
    {.internal = 1500, .addr = PACK_CAN_ADDR, .type = 0x51,  .max_len = 0,   },    // pack单包主要信息
    {.internal = 6000, .addr = PACK_CAN_ADDR, .type = 0x52,  .max_len = 0, },    // pack单包能量信息
    {.internal = 6000, .addr = PACK_CAN_ADDR, .type = 0x54,  .max_len = 0,  },    // pack关于设备
    {.internal = 600,  .addr = PACK_CAN_ADDR, .type = 0x55,  .max_len = 0, },    // pack配置配置
    {.internal = 5000, .addr = PACK_CAN_ADDR, .type = 0x56,  .max_len = 0, },    // pack电芯调试数据
};

static const data_abstract_struct  g_abstract_array_iot[]  = {   /* iot */
    {.internal = 0, .addr = IOT_CAN_ADDR, .type = 0x00,  .max_len = sizeof(iot_base_struct) - sizeof(rw_cmd_struct),   },    // iot基本数据信息
    {.internal = 0, .addr = IOT_CAN_ADDR, .type = 0x01,  .max_len = sizeof(iot_config_struct) - sizeof(rw_cmd_struct), },    // iot配置信息
    {.internal = 0, .addr = IOT_CAN_ADDR, .type = 0x02,  .max_len = sizeof(iot_about_struct) - sizeof(rw_cmd_struct),  },    // iot出厂信息
    {.internal = 0, .addr = IOT_CAN_ADDR, .type = 0x03,  .max_len = sizeof(iot_wifi_struct) - sizeof(rw_cmd_struct),   },    // WiFi信息
    {.internal = 0, .addr = IOT_CAN_ADDR, .type = 0x04,  .max_len = sizeof(iot_ble_struct) - sizeof(rw_cmd_struct),    },    // 蓝牙信息
    {.internal = 0, .addr = IOT_CAN_ADDR, .type = 0x05,  .max_len = sizeof(iot_4g_struct) - sizeof(rw_cmd_struct),     },    // 4G信息
    {.internal = 0, .addr = IOT_CAN_ADDR, .type = 0x06,  .max_len = sizeof(iot_local_struct) - sizeof(rw_cmd_struct),  },    // iot定位信息
};

// static const data_abstract_struct  g_factory_array[] = {
//     {.internal = 1000, .addr = 0xFF, .type = 0xFF, .max_len = sizeof(factory_data_t),  },    // 标定区
// };

can_cmd_array g_cmd_array_inv[] = {  /* 时间间隔为ms，时间精度为50ms */
    {.is_auto = false, .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_inv[0]},    // 逆变广播数据
    {.is_auto = true,  .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_inv[1]},    // 逆变汇总后数据
    {.is_auto = true,  .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_inv[2]},    // 能量线
    {.is_auto = true,  .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_inv[3]},    // 关于逆变
    {.is_auto = true,  .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_inv[4]},    // pv输入信息
    {.is_auto = true,  .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_inv[5]},    // 电网信息
    {.is_auto = true,  .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_inv[6]},    // 负载信息
    {.is_auto = true,  .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_inv[7]},    // 逆变信息
    {.is_auto = true,  .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_inv[8]},    // 电表信息
    {.is_auto = true,  .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_inv[9]},    // 发电机信息
    {.is_auto = true,  .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_inv[10]},   // 设置数据00
    {.is_auto = true,  .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_inv[11]},   // 设置数据01
    {.is_auto = true,  .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_inv[12]},   // 设置数据02
    {.is_auto = true,  .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_inv[13]},   // 设置数据03
    {.is_auto = false, .is_expire = 0, .left_time = 0,  .used_cnt = 0x02,  .usable_cnt = 0x02, .abstract = &g_abstract_array_inv[14]},   // 历史记录      触发读取(每次触发读取1次)
    {.is_auto = false, .is_expire = 0, .left_time = 0,  .used_cnt = 0x02,  .usable_cnt = 0x02, .abstract = &g_abstract_array_inv[15]},   // 往年总电量信息 触发读取(每次触发读取2次)
    {.is_auto = false, .is_expire = 0, .left_time = 0,  .used_cnt = 0x02,  .usable_cnt = 0x02, .abstract = &g_abstract_array_inv[16]},   // 今年总电量信息 触发读取(每次触发读取2次)
    {.is_auto = true,  .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_inv[17]},   // 第3方wifi信息
    {.is_auto = true,  .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_inv[18]},   // 上报给外置wifi的数据
    {.is_auto = true,  .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_inv[19]},   // 外置wifi下发的数据
};

can_cmd_array g_cmd_array_pack[] = { /* 时间间隔为ms，时间精度为50ms */
    {.is_auto = false,.is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_pack[0]},    // pack主包广播
    {.is_auto = true, .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_pack[1]},    // pack单包主要信
    {.is_auto = true, .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_pack[2]},    // pack单包扩展信
    // {.is_auto = true, .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_pack[3]},    // pack故障信息
    {.is_auto = true, .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_pack[3]},    // pack关于设备
    {.is_auto = true, .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_pack[4]},    // pack配置
    {.is_auto = true, .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_pack[5]},    // pack调试数据
};

can_cmd_array g_cmd_array_iot[] = {  /* 时间间隔为ms，时间精度为50ms */
    {.is_auto = false,.is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_iot[0]},    // iot基本数据信息
    {.is_auto = true, .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_iot[1]},    // iot配置信息
    {.is_auto = true, .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_iot[2]},    // iot出厂信息
    {.is_auto = true, .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_iot[3]},    // WiFi信息
    {.is_auto = true, .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_iot[4]},    // 蓝牙信息
    {.is_auto = true, .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_iot[5]},    // 4G信息
    {.is_auto = true, .is_expire = 1, .left_time = 0,  .used_cnt = 0x00,  .usable_cnt = 0xFF, .abstract = &g_abstract_array_iot[6]},    // iot定位信息
};

const uint8_t inv_cmd_count  = sizeof(g_cmd_array_inv) / sizeof(g_cmd_array_inv[0]);
const uint8_t pack_cmd_count = sizeof(g_cmd_array_pack) / sizeof(g_cmd_array_pack[0]);
const uint8_t iot_cmd_count  = sizeof(g_cmd_array_iot) / sizeof(g_cmd_array_iot[0]);

void CanCmdPackReset(void) {
    for (uint8_t i =0; i < pack_cmd_count; i++) {
        g_cmd_array_pack[i].is_expire = 1;
    }
}

void CanCmdInvReset(void) {
    for (uint8_t i =0; i < inv_cmd_count; i++) {
        g_cmd_array_inv[i].is_expire = 1;
    }
}

void CanCmdActivte(uint8_t cmd_type) {

    for (uint8_t i =0; i < inv_cmd_count; i++) { /*  */
        if (g_cmd_array_inv[i].is_auto == false &&
            g_cmd_array_inv[i].abstract->type == cmd_type) {
            g_cmd_array_inv[i].used_cnt = 0;
            g_cmd_array_inv[i].left_time = 0;
            g_cmd_array_inv[i].is_expire = 1;
            break;
        }
    }

    for (uint8_t i =0; i < pack_cmd_count; i++) {
        if (g_cmd_array_pack[i].is_auto == false &&
            g_cmd_array_pack[i].abstract->type == cmd_type) {
            g_cmd_array_pack[i].used_cnt = 0;
            g_cmd_array_pack[i].left_time = 0;
            g_cmd_array_pack[i].is_expire = 1;
            break;
        }
    }

}

/*

CAN 写 和 modbus写 ，使用相同标志，统一到同一执行位置
*/
void Can_iot_callback_set_type_0x2(    uint16_t write_offset, uint16_t data_len)
{
	uint32_t address_begin = (uint32_t)&Inv_can_WR.mod_IOT_set.mod_reg12000_IOT_set + write_offset;
	uint32_t address_end = (uint32_t)&Inv_can_WR.mod_IOT_set.mod_reg12000_IOT_set + write_offset + data_len;
	iot_can_node_struct_reg12000 *Ponter;

	Ponter = &Inv_can_WR.mod_IOT_set.mod_reg12000_IOT_set;

	if((0 == write_offset)
		&&((sizeof(iot_can_node_struct_reg12000_mini)*2) == data_len))//如果收到其他设备的因SN变化的IOT参数同步，则本机不在主动同步（WR）
	{
		reals.T_delay_iot_para_can_wr =-1;

	}

	if((address_begin <= (uint32_t)&Ponter->wifi_sta_ssid)
		&&(address_end >= (uint32_t)&Ponter->wifi_sta_ssid))
	{
		reals.ModbusCmdFlag.sBit.wifi_sta_ssid = 1;

		memcpy((uint8_t *)&Inv_WR.mod_reg12000_IOT_set.wifi_sta_ssid, (uint8_t *)&Ponter->wifi_sta_ssid, sizeof(Ponter->wifi_sta_ssid));

	}
	if((address_begin <= (uint32_t)&Ponter->wifi_sta_password)
		&&(address_end >= (uint32_t)&Ponter->wifi_sta_password))
	{
		reals.ModbusCmdFlag.sBit.wifi_sta_password = 1;
		memcpy((uint8_t *)&Inv_WR.mod_reg12000_IOT_set.wifi_sta_password , (uint8_t *)&Ponter->wifi_sta_password, sizeof(Ponter->wifi_sta_password));

	}

	if((address_begin <= (uint32_t)&Ponter->wifi_sta_auth)
		&&(address_end >= (uint32_t)&Ponter->wifi_sta_auth))
	{
		reals.ModbusCmdFlag.sBit.wifi_sta_auth = 1;
		memcpy((uint8_t *)&Inv_WR.mod_reg12000_IOT_set.wifi_sta_auth , (uint8_t *)&Ponter->wifi_sta_auth, sizeof(Ponter->wifi_sta_auth));

	}


	if((address_begin <= (uint32_t)&Ponter->could_dns[0])
		&&(address_end >= (uint32_t)&Ponter->could_dns[0]))
	{
		reals.ModbusCmdFlag.sBit.could_dns = 1;
		memcpy((uint8_t *)&Inv_WR.mod_reg12000_IOT_set.could_dns[0] , (uint8_t *)&Ponter->could_dns[0], sizeof(Ponter->could_dns[0]));

	}
	if((address_begin <= (uint32_t)&Ponter->mobile_apn[0])
		&&(address_end >= (uint32_t)&Ponter->mobile_apn[0]))
	{
		reals.ModbusCmdFlag.sBit.mobile_apn = 1;
		memcpy((uint8_t *)&Inv_WR.mod_reg12000_IOT_set.mobile_apn[0] , (uint8_t *)&Ponter->mobile_apn[0], sizeof(Ponter->mobile_apn[0]));

	}
	if((address_begin <= (uint32_t)&Ponter->on_off)
		&&(address_end >= (uint32_t)&Ponter->on_off))
	{
		reals.ModbusCmdFlag.sBit.on_off = 1;
		memcpy((uint8_t *)&Inv_WR.mod_reg12000_IOT_set.on_off , (uint8_t *)&Ponter->on_off, sizeof(Ponter->on_off));
	}
	if((address_begin <= (uint32_t)&Ponter->thunder_ctrl)
		&&(address_end >= (uint32_t)&Ponter->thunder_ctrl))
	{
		reals.ModbusCmdFlag.sBit.thunder_ctrl = 1;
		memcpy((uint8_t *)&Inv_WR.mod_reg12000_IOT_set.thunder_ctrl , (uint8_t *)&Ponter->thunder_ctrl, sizeof(Ponter->thunder_ctrl));

	}
	if((address_begin <= (uint32_t)&Ponter->period_report)
		&&(address_end >= (uint32_t)&Ponter->period_report))
	{
		reals.ModbusCmdFlag.sBit.period_report = 1;
		memcpy((uint8_t *)&Inv_WR.mod_reg12000_IOT_set.period_report , (uint8_t *)&Ponter->period_report, sizeof(Ponter->period_report));

	}
	if((address_begin <= (uint32_t)&Ponter->IOT_Enable_mix1)
		&&(address_end >= (uint32_t)&Ponter->IOT_Enable_mix1))
	{
		reals.ModbusCmdFlag.sBit.IOT_Enable_mix1 = 1;
		memcpy((uint8_t *)&Inv_WR.mod_reg12000_IOT_set.IOT_Enable_mix1 , (uint8_t *)&Ponter->IOT_Enable_mix1, sizeof(Ponter->IOT_Enable_mix1));

	}
	if((address_begin <= (uint32_t)&Ponter->IOT_Enable_mix2)
		&&(address_end >= (uint32_t)&Ponter->IOT_Enable_mix2))
	{
		reals.ModbusCmdFlag.sBit.IOT_Enable_mix2 = 1;
		memcpy((uint8_t *)&Inv_WR.mod_reg12000_IOT_set.IOT_Enable_mix2 , (uint8_t *)&Ponter->IOT_Enable_mix2, sizeof(Ponter->IOT_Enable_mix2));

	}
	if((address_begin <= (uint32_t)&Ponter->Protocol_3r_Enable_mix1)
		&&(address_end >= (uint32_t)&Ponter->Protocol_3r_Enable_mix1))
	{
		reals.ModbusCmdFlag.sBit.Protocol_3r_Enable_mix1 = 1;
		memcpy((uint8_t *)&Inv_WR.mod_reg12000_IOT_set.Protocol_3r_Enable_mix1 , (uint8_t *)&Ponter->Protocol_3r_Enable_mix1, sizeof(Ponter->Protocol_3r_Enable_mix1));

	}
	if((address_begin <= (uint32_t)&Ponter->Protocol_3r_Enable_mix2)
		&&(address_end >= (uint32_t)&Ponter->Protocol_3r_Enable_mix2))
	{
		reals.ModbusCmdFlag.sBit.Protocol_3r_Enable_mix2 = 1;
		memcpy((uint8_t *)&Inv_WR.mod_reg12000_IOT_set.Protocol_3r_Enable_mix2 , (uint8_t *)&Ponter->Protocol_3r_Enable_mix2, sizeof(Ponter->Protocol_3r_Enable_mix2));

	}

}

/*------------------------------------------------------------------------
modbus iot clean
*/
void modbus_iot_value_clean(uint8_t num)
{
//IOT
	ESP_LOGE(TAG, "modbus_iot_value_clean:%d",num);// iot数据清零
	//memset(&Inv[num].mod_reg00000, 0x00, sizeof(MOD_STRUCT_reg00000));
	memset(&Inv[num].mod_reg00000.support_mode, 0x00, sizeof(iot_mode_struct));
	memset(&Inv[num].mod_reg00000.app_password, 0x00, sizeof(Inv[num].mod_reg00000.app_password));
	Inv[num].mod_reg00000.modbus_ver=0;

	memset(&Inv[num].mod_reg00700_OTA, 0x00, sizeof(MOD_STRUCT_reg00700));
	memset(&Inv[num].mod_reg11000_IOT_info, 0x00, sizeof(MOD_STRUCT_reg11000));
	memset(&Inv[num].mod_reg12000_IOT_set, 0x00, sizeof(MOD_STRUCT_reg12000));
	memset(&Inv[num].mod_reg13500_mesh, 0x00, sizeof(MOD_STRUCT_reg13500));
    memset(&Inv[num].mod_reg13600_open, 0x00, sizeof(MOD_STRUCT_reg13600));
	memset(&Inv[num].mod_reg21000_bind, 0x00, sizeof(MOD_STRUCT_reg21000));
	memset(&Inv[num].mod_reg21000_bind_WR, 0x00, sizeof(MOD_STRUCT_reg21000_WR));
	memset(&Inv[num].mod_reg22000_net_server_2rd, 0x00, sizeof(MOD_STRUCT_reg22000));
	memset(&Inv[num].mod_reg29700_IOT_info, 0x00, sizeof(MOD_STRUCT_reg29700));

}

/*------------------------------------------------------------------------
modbus inv clean
*/
void modbus_inv_value_clean(uint8_t num)
{
	//INV
	 //ESP_LOGE(TAG, "modbus_inv_value_clean:%d",num);// inv数据清零
	memset(&Inv[num].mod_reg00100_AppPage1, 0x00, sizeof(MOD_STRUCT_reg00100));
	if (reals.IOT_Status_Flag.sBit.system_sleep_flag) {
		/* 休眠期间清零后保留休眠显示字段，否则App蓝牙重连读到全0不显示休眠 */
		Inv[num].mod_reg00100_AppPage1.Iot_State.bit.remote_off_state = 1;
		Inv[num].mod_reg00100_AppPage1.Parts_online.bit.remote_off_en = 1;
		iot_sleep_ctx_restore_mod_reg00100(&Inv[num].mod_reg00100_AppPage1);
	}
	memset(&Inv[num].mod_reg01100_Inv_base, 0x00, sizeof(MOD_STRUCT_reg01100));
	memset(&Inv[num].mod_reg01200_Inv_pv, 0x00, sizeof(MOD_STRUCT_reg01200));

	memset(&Inv[num].mod_reg01300_Inv_grid, 0x00, sizeof(MOD_STRUCT_reg01300));
	memset(&Inv[num].mod_reg01400_Inv_load, 0x00, sizeof(MOD_STRUCT_reg01400));
	memset(&Inv[num].mod_reg01500_Inv_inv, 0x00, sizeof(MOD_STRUCT_reg01500));
	memset(&Inv[num].mod_reg01600_Inv_generator, 0x00, sizeof(MOD_STRUCT_reg01600));
	memset(&Inv[num].mod_reg02000_Inv_base_set, 0x00, sizeof(MOD_STRUCT_reg02000));

	memset(&Inv[num].mod_reg02200_Inv_advance_set, 0x00, sizeof(MOD_STRUCT_reg02200));
	memset(&Inv[num].mod_reg02300_Inv_set02_struct, 0x00, sizeof(MOD_STRUCT_reg02300));
	memset(&Inv[num].mod_reg02400_Inv_certification, 0x00, sizeof(MOD_STRUCT_reg02400));
	memset(&Inv[num].mod_reg02500_Inv_advance_set2, 0x00, sizeof(MOD_STRUCT_reg02500));
	memset(&Inv[num].mod_reg03000_Inv_history, 0x00, sizeof(MOD_STRUCT_reg03000));

	memset(&Inv[num].mod_reg03500_Inv_yearX_statistic, 0x00, sizeof(MOD_STRUCT_reg03500));
	memset(&Inv[num].mod_reg03600_Inv_year1_statistic, 0x00, sizeof(MOD_STRUCT_reg03600));
	memset(&Inv[num].mod_reg04000_Dsp_data, 0x00, sizeof(MOD_STRUCT_reg04000));
	memset(&Inv[num].mod_reg04050_Dsp_set1, 0x00, sizeof(MOD_STRUCT_reg04050));
	memset(&Inv[num].mod_reg04105_Dsp_set2, 0x00, sizeof(MOD_STRUCT_reg04105));

	memset(&Inv[num].mod_reg06000_Pack_sum, 0x00, sizeof(MOD_STRUCT_reg06000));
	memset(&Inv[num].mod_reg06100_Pack_each, 0x00, sizeof(MOD_STRUCT_reg06100));
	memset(&Inv[num].mod_reg07000_Pack_set, 0x00, sizeof(MOD_STRUCT_reg07000));

	memset(&Inv[num].mod_reg13000_3rd_WIFI, 0x00, sizeof(MOD_STRUCT_reg13000));
	memset(&Inv[num].mod_reg14500_SmartPlug_info, 0x00, sizeof(MOD_STRUCT_reg14500));
	memset(&Inv[num].mod_reg14700_SmartPlug_set, 0x00, sizeof(MOD_STRUCT_reg14700));
	memset(&Inv[num].mod_reg15500_D400s_info, 0x00, sizeof(MOD_STRUCT_reg15500));
	memset(&Inv[num].mod_reg15600_D400s_set, 0x00, sizeof(MOD_STRUCT_reg15600));
	memset(&Inv[num].mod_reg40000_transparent, 0x00, sizeof(MOD_STRUCT_reg40000));

}


/*------------------------------------------------------------------------------
 Function: update_online_counts
 -----------------------------------------------------------------------------*/
/**
  * @brief      更新在线设备数
  * @param[in]  int temp_iot_online_cnt
                int temp_inv_online_cnt
                int temp_pack_online_cnt
                int temp_achub_online_cnt
                int temp_dchub_online_cnt
                int temp_panel_online_cnt
  * @param[out] None
  * @return     void
  */
void update_online_counts(int temp_iot_online_cnt, int temp_inv_online_cnt, int temp_pack_online_cnt,
                                    int temp_achub_online_cnt, int temp_dchub_online_cnt, int temp_d400s_online_cnt,int temp_at1_online_cnt)
{
    // 保存旧值
    int old_online_Iot_num = reals.online_Iot_num;
    int old_online_Inv_num = reals.online_Inv_num;
    int old_online_Pack_num = reals.online_Pack_num;
    int old_online_ACHUB_num = reals.online_ACHUB_num;
    int old_online_DCHUB_num = reals.online_DCHUB_num;
	int old_online_D400S_num = reals.online_D400S_num;
	int old_online_AT1_num =reals.online_AT1_num;
    // 更新值
    reals.online_Iot_num = temp_iot_online_cnt + 1; // 包括自己
    reals.online_Inv_num = temp_inv_online_cnt;
    reals.online_Pack_num = temp_pack_online_cnt;
    reals.online_ACHUB_num = temp_achub_online_cnt;
    reals.online_DCHUB_num = temp_dchub_online_cnt;
	reals.online_D400S_num =temp_d400s_online_cnt;
	reals.online_AT1_num=temp_at1_online_cnt;

    if (temp_pack_online_cnt > old_online_Pack_num) {
        reals.need_new_version_flag.sBit.pack = 1;
    }

    // 检查是否有值发生变动,
    bool changed = (reals.online_Iot_num != old_online_Iot_num) ||
                   (reals.online_Inv_num != old_online_Inv_num) ||
                   (reals.online_Pack_num != old_online_Pack_num) ||
                   (reals.online_ACHUB_num != old_online_ACHUB_num) ||
                   (reals.online_DCHUB_num != old_online_DCHUB_num)||
				    (reals.online_D400S_num != old_online_D400S_num)||
				   (reals.online_AT1_num != old_online_AT1_num);
	//ESP_LOGI(TAG,"New Node old iot:%d inv:%d pack:%d achub:%d dchub:%d ",old_online_Iot_num,old_online_Inv_num,old_online_Pack_num,old_online_ACHUB_num,old_online_DCHUB_num);
	//ESP_LOGI(TAG,"New Node num iot:%d inv:%d pack:%d achub:%d dchub:%d ",reals.online_Iot_num,reals.online_Inv_num,reals.online_Pack_num,reals.online_ACHUB_num,reals.online_DCHUB_num);
    // 设置标志
    if (changed) {
        reals.net_point_Comein = 1;
    }
}


/*------------------------------------------------------------------------
*@Function： CanNodeOfflineCheck
CAN节点通信超时清零


alive_time 的赋值在CanDevIdCheck()，收到CAN ID即赋值

-------------------------------------------------------------------------*/
/**
*@brief
*@param[in]     None
*@param[out]    None
*@return
*/

void CanNodeOfflineCheck(uint16_t interval)
{
	uint8_t tempsubcnt = 0;
	uint8_t tempsubcnt_dchub_bit = 0;//dchub按位表示在线状态，方便OTA升级时DCHUB离线也能知道是升级的哪几个
	uint8_t tempsubcnt_dchub_online = 0;//dchub实际在线计数器
	uint8_t tempsubcnt_d400s_bit = 0;//d400s按位表示在线状态，方便OTA升级时DCHUB离线也能知道是升级的哪几个
	uint8_t tempsubcnt_d400s_online = 0;//d400s实际在线计数器

	uint8_t tempsubcnt_inv_bit = 0;//inv按位表示在线状态 传递寄存器121 并机时app会根据该值来读对应逆变的1100段数据，读到才允许进入设置区
	uint8_t temp_inv_online_cnt = 0;//
	uint8_t temp_pack_online_cnt = 0;//
	uint8_t temp_achub_online_cnt = 0;//
	uint8_t temp_iot_online_cnt = 0;//
	uint8_t temp_at1_online_cnt = 0;//
	//uint8_t temp_dchub_online_cnt = 0;
	//uint8_t temp_d400s_online_cnt = 0;

//	static uint8_t scnt_pack = 0;
    uint16_t temp_online =0;

	/*升级时候，保持最后的设备在线数量，不更新*/
	if(1 == reals.ota_happen) {
		return;
	}
	if(reals.ota_happen == 0)
	{
//return;//debug
		for(int node = 0; node < DEV_MAIN_NODE_MAX; node++)
		{
			tempsubcnt = 0;

			//inv超时判断
			for (uint8_t i = 0; i < (INV_MAX_NUM); i++)
			{

				if(reals.Step_can_dev_parallel < STEP_CAN_PARALLEL_AFTER)//
				{
					Inv_can[node].inv_data[i].setdata_valid=0;
				}

				if (Inv_can[node].inv_data[i].online == 1)
				{
					temp_inv_online_cnt++;
					tempsubcnt_inv_bit |= (uint8_t)1 << node;//对应位置一

					Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_announce.online|= (uint16_t)1<<(node*INV_MAX_NUM +i); 					// 汇总逆变在线标志
					Inv_can[node].inv_data[INV_MAX_NUM].inv_announce.online|= (uint16_t)1<<(i); 					// 汇总逆变在线标志


					if (Inv_can[node].inv_data[i].alive_time >= interval)
					{
						Inv_can[node].inv_data[i].alive_time -= interval;
					}
					else
					{
						Inv_can[node].inv_data[i].alive_time = 0;
						Inv_can[node].inv_data[i].online = 0;
						memset(&Inv_can[node].inv_data[i], 0x00, sizeof(Inv_can[node].inv_data[i]));

						ESP_LOGE(TAG, "inv 0x%x offline", i);
					}
				}
				else
				{
					tempsubcnt++;
					Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_announce.online&= ~((uint16_t)1<<(node*INV_MAX_NUM +i)); 					// 汇总逆变在线标志
					Inv_can[node].inv_data[INV_MAX_NUM].inv_announce.online&= ~((uint16_t)1<<(i)); 					// 汇总逆变在线标志

	//bat clean，因电池信息是通过INV 上报，INV丢失，则BAT丢失
					Inv_can[node].pack_data[0].pack_announce.online =0;
					for (uint8_t j = 0; j < (PACK_MAX_NUM); j++)
					{
						Inv_can[node].pack_data[j].online =0;
						Inv_can[node].pack_data[j].alive_time = 0;
					}
				}
			}

			if(tempsubcnt >= INV_MAX_NUM)//单系统的 所有INV掉线
			{
				Inv_can[node].inv_data[0].inv_announce.online = 0; //清除逆变在线标记
				Inv_can[node].inv_data[INV_MAX_NUM].inv_announce.online = 0; //清除逆变在线标记
				memset(&Inv_can[node].inv_data[INV_MAX_NUM], 0x00, sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM]));

	//			memset(&Inv[node], 0x00, sizeof(Inv[node]));

				modbus_inv_value_clean(node);





			}

			tempsubcnt =0;
			//pack超时判断
			for (uint8_t i = 0; i < (PACK_MAX_NUM); i++)
			{
				//ESP_LOGI(TAG,"Inv_can[%d].pack_data[0].pack_announce.online:%d",node,Inv_can[node].pack_data[0].pack_announce.online);
				if(Inv_can[node].pack_data[0].pack_announce.online & (((uint16_t)1 << i)))
				{
					Inv_can[node].pack_data[i].online =1;
					Inv_can[node].pack_data[i].alive_time = PACK_OFFLINE_TIME;
					temp_pack_online_cnt++;

				}

				if (Inv_can[node].pack_data[i].alive_time >= interval)
				{
					if(!Inv_can[node].pack_data[i].online)
					{
						Inv_can[node].pack_data[i].online =1;
						temp_pack_online_cnt++;
					}
					Inv_can[node].pack_data[i].alive_time -= interval;
				}
				else
				{
					Inv_can[node].pack_data[i].alive_time = 0;
					Inv_can[node].pack_data[i].online = 0;
					if(0 == i)
					{
						temp_online =Inv_can[node].pack_data[i].pack_announce.online;
					}
					memset(&Inv_can[node].pack_data[i], 0x00, sizeof(Inv_can[node].pack_data[i]));
					if(0 == i)
					{
						Inv_can[node].pack_data[i].pack_announce.online =temp_online;
					}


					//ESP_LOGE(TAG, "pack 0x%x offline", i);
					tempsubcnt++;
					if(i==0)
					{
						//memset(&Inv_Pack[node*PACK_MAX_NUM+ i], 0x00, sizeof(Inv_Pack[0]));
						//逻辑修改，Inv_Pack（00~03）保存逆变器内置电池包数据，只判断3个逆变器内置电池包是否在线
						memset(&Inv_Pack[node], 0x00, sizeof(Inv_Pack[0]));
					}

				}
			}

			if(tempsubcnt >= PACK_MAX_NUM)//单系统的 所有PACK掉线
			{
				/* pack主机掉线后, 需要清除主机的广播数据 */
				memset(&Inv_can[node].pack_data[0].pack_announce, 0, sizeof(pack_announce_struct) - sizeof(rw_cmd_struct));
				memset(&Inv_can[node].pack_data[PACK_MAX_NUM].pack_announce, 0, sizeof(pack_announce_struct) - sizeof(rw_cmd_struct));
				memset(&Inv_can[node].pack_data[PACK_MAX_NUM], 0x00, sizeof(Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM]));


			}
			tempsubcnt =0;

			//iot超时判断
			for (uint8_t i = 0; i < (IOT_MAX_NUM); i++)
			{
				if( (Inv_can[node].iot_data[i].online == 1)
					&&(node != reals.Addr_can_self))
				{
					temp_iot_online_cnt++;
					if (Inv_can[node].iot_data[i].alive_time >= interval)
					{
						Inv_can[node].iot_data[i].alive_time -= interval;
					}
					else
					{
						Inv_can[node].iot_data[i].alive_time = 0;
						Inv_can[node].iot_data[i].online = 0;
						memset(&Inv_can[node].iot_data[i], 0x00, sizeof(Inv_can[node].iot_data[i]));

						modbus_iot_value_clean(node);
						ESP_LOGE(TAG, "iot	offline");
					}
				}
			}

			//d400s超时判断
			for (uint8_t i = 0; i < (D400S_MAX_NUM); i++)
			{
				//ESP_LOGI(TAG,"Inv_can[%d].d400s_data[%d].online:%d",node,i,Inv_can[node].d400s_data[i].online);
				if (Inv_can[node].d400s_data[i].online == 1)
				{
					tempsubcnt_d400s_online++;
					tempsubcnt_d400s_bit |= (uint8_t)1 << node;//对应位置一
					if (Inv_can[node].d400s_data[i].alive_time >= interval)
					{
						Inv_can[node].d400s_data[i].alive_time -= interval;
					}
					else
					{
						Inv_can[node].d400s_data[i].alive_time = 0;
						Inv_can[node].d400s_data[i].online = 0;
						memset(&Inv_can[node].d400s_data[i], 0x00, sizeof(Inv_can[node].d400s_data[i]));

						ESP_LOGE(TAG, "d400s 0x%x offline", i);

						memset(&Inv[node].mod_reg15500_D400s_info, 0x00, sizeof(MOD_STRUCT_reg15500));
						memset(&Inv[node].mod_reg15600_D400s_set, 0x00, sizeof(MOD_STRUCT_reg15600));
					}
				}
			}

			//dchub超时判断
			for (uint8_t i = 0; i < (DC_HUB_MAX_NUM); i++)
			{
				if (Inv_can[node].dc_hub_data[i].online == 1)
				{
					tempsubcnt_dchub_online++;
					tempsubcnt_dchub_bit |= (uint8_t)1 << node;//对应位置一
					if (Inv_can[node].dc_hub_data[i].alive_time >= interval)
					{
						Inv_can[node].dc_hub_data[i].alive_time -= interval;
					}
					else
					{
						Inv_can[node].dc_hub_data[i].alive_time = 0;
						Inv_can[node].dc_hub_data[i].online = 0;
						memset(&Inv_can[node].dc_hub_data[i], 0x00, sizeof(Inv_can[node].dc_hub_data[i]));

						ESP_LOGE(TAG, "dc_hub 0x%x offline", i);

						memset(&Inv[node].mod_reg15700_Dc_Hub_info, 0x00, sizeof(MOD_STRUCT_reg15700));
					}
				}
			}

		}

		//achub超时判断
		for (uint8_t i = 0; i < (AC_HUB_MAX_NUM); i++)
		{
			if (Inv_can_mix.ac_hub_data[i].online == 1)
			{
				//ESP_LOGE(TAG, "Inv_can_mix.ac_hub_data[%d].alive_time:%d", i,(unsigned int)Inv_can_mix.ac_hub_data[i].alive_time);
				temp_achub_online_cnt = 1; // achub只有一个
				if (Inv_can_mix.ac_hub_data[i].alive_time >= interval)
				{
					Inv_can_mix.ac_hub_data[i].alive_time -= interval;
				}
				else
				{
					Inv_can_mix.ac_hub_data[i].alive_time = 0;
					Inv_can_mix.ac_hub_data[i].online = 0;
					memset(&Inv_can_mix.ac_hub_data[i], 0x00, sizeof(Inv_can_mix.ac_hub_data[i]));
					reals.If_AC_HUB_SingleBoot = 0;//achub掉线后，单boot状态置零

					ESP_LOGE(TAG, "ac_hub 0x%x offline", i);

					memset(&Inv[INV_MAX_NUM*DEV_MAIN_NODE_MAX].mod_reg15800_Ac_Hub_info, 0x00, sizeof(Inv_Pack[0]));//testwx
					// Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.Parts_online &= ~(1 << 1);//ACHub全部掉线后，该位置零  -> 改为开关状态

					//当检测到AC_HUB离线，且此时MQTT登录的类型不为IOT时(单机状态)，主IOT重新以IOT及底层逆变的信息启动MQTT，升级时不允许更改(此时实际ACHUB是在线的)
					if((strcmp(login_info.dev_type, IOT_TYPE_IOT) != 0) && (reals.Addr_can_master == 1))
					{
						if(reals.ota_happen == 0)
						{
							ESP_LOGI(TAG, "CanNodeOfflineCheck ACHUB offline, update MQTT state, use single mode");
							//reals.mqttChange_flag=1;
							iot_mqtt_ChageFlagSet(1);
							//iot_wifi_new_iot(iot_factory.iot_type, iot_factory.iot_sn, iot_factory.safe_code);
							//iot_wifi_new_dev(SetData.dev_info_t.INV_dev_type,  SetData.dev_info_t.INV_dev_sn);
						}
						else if(reals.ota_happen == 1)
						{
							ESP_LOGI(TAG, "ota_happen == 1, still use mix mode");
						}

					}
				}
			}
		}

		//at1超时判断
		for (uint8_t i = 0; i < (ATS_MAX_NUM); i++)
		{
			if (Inv_can_mix.ATS_data[i].online == 1)
			{
				ESP_LOGE(TAG, "Inv_can_mix.ac_hub_data[%d].alive_time:%d", i,(unsigned int)Inv_can_mix.ATS_data[i].alive_time);
				temp_at1_online_cnt = 1; // ats只有一个
				if (Inv_can_mix.ATS_data[i].alive_time >= interval)
				{
					Inv_can_mix.ATS_data[i].alive_time -= interval;
				}
				else
				{
					Inv_can_mix.ATS_data[i].alive_time = 0;
					Inv_can_mix.ATS_data[i].online = 0;
					memset(&Inv_can_mix.ATS_data[i], 0x00, sizeof(Inv_can_mix.ATS_data[i]));

					ESP_LOGE(TAG, "ats_hub 0x%x offline", i);

					//memset(&Inv[INV_MAX_NUM*DEV_MAIN_NODE_MAX].mod_reg15800_Ac_Hub_info, 0x00, sizeof(Inv_Pack[0]));//testwx
					// Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.Parts_online &= ~(1 << 1);//ACHub全部掉线后，该位置零  -> 改为开关状态
				}
			}
		}


		if(0 == reals.ota_happen)//升级时候，保持最后的设备在线数量
		{
			/*更新设备数量*/
			update_online_counts(temp_iot_online_cnt, temp_inv_online_cnt, temp_pack_online_cnt,
								temp_achub_online_cnt, tempsubcnt_dchub_online,tempsubcnt_d400s_online,temp_at1_online_cnt);

			reals.online_Inv_bit = tempsubcnt_inv_bit;
			//OTA升级时，DCHUB会离线，靠此记录要升级哪几个设备(实际效果不好，还是得直接靠获取底层最低进度用于表示总进度)；可复用表示哪一台dchub在线
			reals.online_DCHUB_bit = tempsubcnt_dchub_bit;
			reals.online_D400S_bit = tempsubcnt_d400s_bit;



		}


	}

	if(0 == reals.online_DCHUB_num)
	{
		// Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.Parts_online &= ~(1 << 0);//DCHub全部掉线后，该位置零
		Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.Parts_online.bit.dc_hub_online = 0;//DCHub全部掉线后，该位置零
	}

	for (int index = 0; index < INV_MAX_NUM * DEV_MAIN_NODE_MAX; index++)
	{
        if ((reals.online_DCHUB_bit & (1 << index)) == 0)
		{
            Inv[index].mod_reg00100_AppPage1.Parts_online.bit.dc_hub_online = 0;// app使用从机地址获取dchub在线状态
        }else
		{
			 Inv[index].mod_reg00100_AppPage1.Parts_online.bit.dc_hub_online = 1;// app使用从机地址获取dchub在线状态
		}
    }





	if(0 == reals.online_Inv_num)
	{
		memset(&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM], 0x00, sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM]));
//		memset(&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)], 0x00, sizeof(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)]));
		modbus_inv_value_clean(INV_MAX_NUM*DEV_MAIN_NODE_MAX);

	}
	else
	{




	}
	if(0 == reals.online_Pack_num)
	{
		memset(&Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM], 0x00, sizeof(Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM]));
	}




}

/*
缓存命令初始化
*/
void CanCmdUpdate(rw_cmd_struct *cmd, uint8_t devId, uint8_t can_type, uint16_t offset, uint32_t len, uint16_t crc16)
{
    if (!cmd)  return ;
    cmd->write_offset = offset;
    cmd->write_next_seq = 0;
    cmd->write_crc16 = crc16;
    cmd->write_remain_len = len;
    cmd->devId = devId;
    cmd->can_type = can_type;//windy add

}

void CanCmdCountDown(uint16_t interval) { /* 周期性命令倒计时发送 */

    /* 逆变命令倒计时 */
    for (uint8_t i = 0; i < inv_cmd_count; i++) {
        if ((g_cmd_array_inv[i].is_auto == false) &&
            (g_cmd_array_inv[i].used_cnt >= g_cmd_array_inv[i].usable_cnt)) {
            g_cmd_array_inv[i].is_expire = 0; /* 触发读取 */
            continue ;
        }

        (g_cmd_array_inv[i].left_time >= interval) ? (g_cmd_array_inv[i].left_time -= interval) : (g_cmd_array_inv[i].left_time = 0);
        if (!g_cmd_array_inv[i].left_time && g_cmd_array_inv[i].abstract->internal) {
            g_cmd_array_inv[i].left_time = g_cmd_array_inv[i].abstract->internal;
            g_cmd_array_inv[i].is_expire = 1;
        }
    }

    /* Pack命令倒计时 */
    for (uint8_t i = 0; i < pack_cmd_count; i++) {
        if ((g_cmd_array_pack[i].is_auto == false) &&
            (g_cmd_array_pack[i].used_cnt >= g_cmd_array_pack[i].usable_cnt)) {
            g_cmd_array_pack[i].is_expire = 0; /* 触发读取 */
            continue ;
        }
        (g_cmd_array_pack[i].left_time >= interval) ? (g_cmd_array_pack[i].left_time -= interval) : (g_cmd_array_pack[i].left_time = 0);
        if (!g_cmd_array_pack[i].left_time && g_cmd_array_pack[i].abstract->internal) {
            g_cmd_array_pack[i].left_time = g_cmd_array_pack[i].abstract->internal;
            g_cmd_array_pack[i].is_expire = 1;
        }
    }
}


//can
#define PACK_CAN_ADDR_GROUP1           0x60	//pack 并机群组 首地址
#define PACK_CAN_ADDR_GROUP2           0x68	//pack 并机群组 首地址
#define PACK_CAN_ADDR_GROUP3           0x70	//pack 并机群组 首地址
#define PACK_CAN_ADDR_GROUP4           0x78	//pack 并机群组 首地址
#define PACK_CAN_ADDR_GROUP5           0x80	//pack 并机群组 首地址
#define PACK_CAN_ADDR_GROUP6           0x88	//pack 并机群组 首地址
#define PACK_CAN_ADDR_GROUP7           0x90	//pack 并机群组 首地址
#define PACK_CAN_ADDR_GROUP8           0x98	//pack 并机群组 首地址
//modbus
#define PACK_MODBUS_ADDR_GROUP1           41	//pack 并机群组 modbus slave首地址
#define PACK_MODBUS_ADDR_GROUP2           49	//pack 并机群组 modbus slave首地址
#define PACK_MODBUS_ADDR_GROUP3           57	//pack 并机群组 modbus slave首地址
#define PACK_MODBUS_ADDR_GROUP4           65	//pack 并机群组 modbus slave首地址
#define PACK_MODBUS_ADDR_GROUP5           73	//pack 并机群组 modbus slave首地址
#define PACK_MODBUS_ADDR_GROUP6           81	//pack 并机群组 modbus slave首地址
#define PACK_MODBUS_ADDR_GROUP7           89	//pack 并机群组 modbus slave首地址
#define PACK_MODBUS_ADDR_GROUP8           97	//pack 并机群组 modbus slave首地址

/*
windy add
CAN beta ID解析

input:
CanId:
output:
level1_addr:返回的 数组结构体一维序号,从0开始
level2_addr:返回的 数组结构体二维序号,从0开始


return:
0:合法 CAN ID
非0：非法 CAN ID
*/
int8_t Can_ID_Check(uint32_t CanId, uint8_t *level1_addr, uint8_t *level2_addr)
{
    int8_t rtn = 0xFF;

    uint8_t pf_fun = 0;
    uint8_t Id_p = 0;
    uint8_t Id_edp = 0;
    uint8_t Id_dp = 0;

    uint8_t addr_source = 0;
    static uint8_t sSameIDcnt = 0;

	pf_fun =(CanId>>16)&0xFF;
    Id_p = (CanId>>26)&0x7;
    Id_edp = (CanId>>25)&0x1;
    Id_dp = (CanId>>24)&0x1;


	addr_source =CanId&0xFF;

    if (((((pf_fun >= 0x71) && (pf_fun <= 0x82))
		||((pf_fun >= 0x60) && (pf_fun <= 0x66))
		||(0xFA == pf_fun))//IOT
		&&(2 == Id_p)&&(0 == Id_edp)&&(0 == Id_dp))//OTA,block read/write
	||((((pf_fun >= 0x10) && (pf_fun <= 0x15))//INV主动上报old
		||((pf_fun >= 0x9B) && (pf_fun <= 0xA0)))//pack主动上报old
		&&(6 == Id_p)&&(0 == Id_edp)&&(0 == Id_dp))//
	||((pf_fun >= 1) && (pf_fun <= 200)//INV.pack主动上报new
		&&(6 == Id_p)&&(0 == Id_edp)&&(1 == Id_dp))//
		)
	{
		if ((addr_source >= INV_CAN_ADDR) && (addr_source < (INV_CAN_ADDR + DEV_MAIN_NODE_MAX*INV_MAX_NUM)) )
		{
			*level1_addr = (addr_source - INV_CAN_ADDR)/INV_GROUP_MAX_NUM; // 逆变
			*level2_addr = (addr_source - INV_CAN_ADDR)%INV_GROUP_MAX_NUM;
			rtn=0;

			if (!Inv_can[*level1_addr].inv_data[*level2_addr].online) {
				iot_can_subdev_online_rise_notify();
			}
			Inv_can[*level1_addr].inv_data[*level2_addr].online =1;
			Inv_can[*level1_addr].inv_data[*level2_addr].alive_time = INV_OFFLINE_TIME; /* 离线检测时间 */
			reals.can_inv_rx_counter++;
		}
		else if ((addr_source >= DC_HUB_CAN_ADDR) && (addr_source < (DC_HUB_CAN_ADDR + DEV_MAIN_NODE_MAX*DC_HUB_MAX_NUM)) )
		{
			*level1_addr = (addr_source - DC_HUB_CAN_ADDR)/DC_HUB_GROUP_MAX_NUM; // dchub
			*level2_addr = (addr_source - DC_HUB_CAN_ADDR)%DC_HUB_GROUP_MAX_NUM;
			rtn=0;
			ESP_LOGI(TAG,"DCHUB ADDR addr_source :0x%x level1_addr:%d level2_addr:%d ",addr_source,*level1_addr,*level2_addr);
			Inv_can[*level1_addr].dc_hub_data[*level2_addr].online =1;
			Inv_can[*level1_addr].dc_hub_data[*level2_addr].alive_time = DC_HUB_OFFLINE_TIME; /* 离线检测时间 */
			reals.can_x_rx_counter++;
		}
		else if (addr_source == AC_HUB_CAN_ADDR)
		{
			*level1_addr = 0; // achub
			*level2_addr = (addr_source - AC_HUB_CAN_ADDR)%AC_HUB_GROUP_MAX_NUM;
			rtn=0;

			Inv_can_mix.ac_hub_data[*level2_addr].online =1;
			Inv_can_mix.ac_hub_data[*level2_addr].alive_time = AC_HUB_OFFLINE_TIME; /* 离线检测时间 */
			//ESP_LOGI(TAG,"AAA ac_hub_data[*level2_addr].alive_time :%d",(unsigned int)Inv_can_mix.ac_hub_data[*level2_addr].alive_time);
			reals.can_x_rx_counter++;
		}
		else if ((addr_source >= PACK_CAN_ADDR) &&( addr_source < (PACK_CAN_ADDR + DEV_MAIN_NODE_MAX*PACK_MAX_NUM)) )
		{
			*level1_addr = (addr_source - PACK_CAN_ADDR)/PACK_GROUP_MAX_NUM; // PACK
			*level2_addr = (addr_source - PACK_CAN_ADDR)%PACK_GROUP_MAX_NUM; // PACK

			rtn=0;
			Inv_can[*level1_addr].pack_data[*level2_addr].online =1;
			Inv_can[*level1_addr].pack_data[*level2_addr].alive_time = PACK_OFFLINE_TIME; /* 离线检测时间 */
			reals.can_pack_rx_counter++;
		}
		else if((addr_source >= IOT_CAN_ADDR) && (addr_source < (IOT_CAN_ADDR + IOT_MAX_NUM*DEV_MAIN_NODE_MAX)) )
		{
			*level1_addr = (addr_source - IOT_CAN_ADDR)/IOT_GROUP_MAX_NUM; // IOT
			*level2_addr = 0;//not use

			rtn=0;
			Inv_can[*level1_addr].iot_data[*level2_addr].online =1;
			Inv_can[*level1_addr].iot_data[*level2_addr].alive_time = IOT_OFFLINE_TIME; /* 离线检测时间 */
			reals.can_iot_rx_counter++;

			if(((reals.Addr_can_self +IOT_CAN_ADDR) == addr_source)
				&&(0x08FA0000 == (((uint32_t)Id_p<<26)|((uint32_t)Id_edp<<25)|((uint32_t)Id_dp<<24)|((uint32_t)pf_fun<<16)))//暂定只判断部分ID，减少因报文太多、时序混乱的误判
				&&(reals.Step_can_dev_parallel >= STEP_CAN_PARALLEL_AFTER))
			{
				if(sSameIDcnt!=0xff)
				{
					if(++sSameIDcnt >= 3)//重复多次滤波
					{
						sSameIDcnt =0;
						reals.Step_can_dev_parallel = STEP_CAN_GROUP_PARALLEL_TRIGER;
						if(Device_Can_Address_Parallel_SetTriger(STEP_CAN_PARALLEL_INTERVAL)<3)
							sSameIDcnt =0;
						else
							sSameIDcnt =0xff;
						//reals.Addr_can_Attr|=Can_Addr_Parallel_SameAlarm;
						ESP_LOGI(TAG,"STEP_CAN_GROUP_PARALLEL_TRIGER	windy DDD sSameIDcnt:%d",sSameIDcnt);

					}
				}

			}
		}
		else if (addr_source == ATS_CAN_ADDR)
		{
			*level1_addr = 0; // ATS
			*level2_addr = (addr_source - ATS_CAN_ADDR)%ATS_GROUP_MAX_NUM;
			rtn=0;

			Inv_can_mix.ATS_data[*level2_addr].online =1;
			Inv_can_mix.ATS_data[*level2_addr].alive_time = ATS_OFFLINE_TIME; /* 离线检测时间 */
			reals.can_x_rx_counter++;
		}
		else if ((addr_source >= D400S_CAN_ADDR) && (addr_source < (D400S_CAN_ADDR + DEV_MAIN_NODE_MAX*D400S_MAX_NUM)) )
		{
			*level1_addr = (addr_source - D400S_CAN_ADDR)/D400S_MAX_NUM; // D400S
			*level2_addr = (addr_source - D400S_CAN_ADDR)%D400S_MAX_NUM;
			rtn=0;

			Inv_can[*level1_addr].d400s_data[*level2_addr].online =1;
			Inv_can[*level1_addr].d400s_data[*level2_addr].alive_time = D400S_OFFLINE_TIME; /* 离线检测时间 */
			reals.can_x_rx_counter++;
		}
		else if (addr_source == 0x00)
		{
			*level1_addr = 0; // 上位机
			rtn=0;

		}
		else
		{
	        ESP_LOGE(TAG, "unkown can dev source addr: 0x%lx", CanId);
	        rtn= -1;
	    }
    }
	else
	{
        ESP_LOGE(TAG, "unkown CAN ID: 0x%lx", CanId);
        rtn= -1;
    }
    return rtn;
}

/*
判断CAN ID，
输出在线标志和定时器计数器

devId:源地址


return:
0~x:CAN 数组结构体索引
*/
uint8_t CanDevIdCheck(node_info_t *node, uint8_t devId)
{
    uint8_t index = 0xFF;
    if ((devId >= INV_CAN_ADDR) && (devId < (INV_CAN_ADDR + (INV_MAX_NUM*DEV_MAIN_NODE_MAX))) )
	{
        index = (devId - INV_CAN_ADDR)%INV_GROUP_MAX_NUM; // 逆变 第几个逆变不靠index区分，靠前面的level1_addr(包含在node->invs_info里)区分
    }
	else if ((devId >= DC_HUB_CAN_ADDR) && (devId < (DC_HUB_CAN_ADDR + (DC_HUB_MAX_NUM*DEV_MAIN_NODE_MAX))) )
	{
        index = (devId - DC_HUB_CAN_ADDR)%DC_HUB_GROUP_MAX_NUM; // dchub
    }
	else if (devId == AC_HUB_CAN_ADDR)
	{
        index = (devId - AC_HUB_CAN_ADDR)%AC_HUB_MAX_NUM; // achub
    }
	else if ((devId >= PACK_CAN_ADDR) &&( devId < (PACK_CAN_ADDR + (PACK_MAX_NUM*DEV_MAIN_NODE_MAX))) )
	{
        index = (devId - PACK_CAN_ADDR)%PACK_GROUP_MAX_NUM; // PACK
    }
	else if((devId >= IOT_CAN_ADDR) && (devId < (IOT_CAN_ADDR + (IOT_MAX_NUM*DEV_MAIN_NODE_MAX)) ) )
	{
        index = (devId - IOT_CAN_ADDR)%IOT_GROUP_MAX_NUM; // IOT 第几个逆变不靠index区分，靠前面的level1_addr(包含在node->invs_info里)区分

    }
	else if (devId == ATS_CAN_ADDR)
	{
        index = (devId - ATS_CAN_ADDR)%ATS_MAX_NUM; // ATS
    }
	else if ((devId >= D400S_CAN_ADDR) && (devId < (D400S_CAN_ADDR + (D400S_MAX_NUM*DEV_MAIN_NODE_MAX))) )
	{
        index = (devId - D400S_CAN_ADDR)%D400S_GROUP_MAX_NUM; // d400s
    }
	else if (devId == 0x00)
	{
        index = 0; // 上位机
    }
	else
	{
        ESP_LOGE(TAG, "CanDevIdCheck: unkown dev addr: 0x%x", devId);
    }
    return index;
}


/*
windy add
CAN beta ID解析

input:
CanId:
output:

return:
0:合法 CAN ID
非0：非法 CAN ID
*/
int8_t Can_ID_Check_Beta_Multi_Read(uint32_t CanId)
{
	int8_t rtn = 0xFF;

	uint8_t pf_fun = 0;
	uint8_t Id_p = 0;
	uint8_t Id_edp = 0;
	uint8_t Id_dp = 0;

	uint8_t addr_source = 0;

	pf_fun =(CanId>>16)&0xFF;
	Id_p = (CanId>>26)&0x7;
	Id_edp = (CanId>>25)&0x1;
	Id_dp = (CanId>>24)&0x1;
	if (((pf_fun >= 0x63) && (pf_fun <= 0x66))
	&&(2 == Id_p)&&(0 == Id_edp)&&(0 == Id_dp))//OTA,block read/write
	{
		rtn = 0;//valid
	}
	else
	{
		rtn = 1;
	}
	return rtn;
}


/*
windy add
CAN beta ID解析

input:
CanId:
output:

return:
0:合法 CAN ID
非0：非法 CAN ID
*/
int8_t Can_ID_Check_Beta_Multi_Write(uint32_t CanId)
{
	int8_t rtn = 0xFF;

	uint8_t pf_fun = 0;
	uint8_t Id_p = 0;
	uint8_t Id_edp = 0;
	uint8_t Id_dp = 0;

	uint8_t addr_source = 0;

	pf_fun =(CanId>>16)&0xFF;
	Id_p = (CanId>>26)&0x7;
	Id_edp = (CanId>>25)&0x1;
	Id_dp = (CanId>>24)&0x1;

if (((pf_fun >= 0x60) && (pf_fun <= 0x62))
	&&(2 == Id_p)&&(0 == Id_edp)&&(0 == Id_dp))//OTA,block read/write
	{
		rtn = 0;//valid
	}
	else
	{
		rtn = 1;
	}
return rtn;
}



/*
windy add
CAN beta ID解析

input:
CanId:
output:

return:
0:合法 CAN ID
非0：非法 CAN ID
*/
int8_t Can_ID_Check_Beta_Multi_OTA(uint32_t CanId)
{
	int8_t rtn = 0xFF;

	uint8_t pf_fun = 0;
	uint8_t Id_p = 0;
	uint8_t Id_edp = 0;
	uint8_t Id_dp = 0;

	uint8_t addr_source = 0;

	pf_fun =(CanId>>16)&0xFF;
	Id_p = (CanId>>26)&0x7;
	Id_edp = (CanId>>25)&0x1;
	Id_dp = (CanId>>24)&0x1;

if (((pf_fun >= 0x71) && (pf_fun <= 0x82))
	&&(2 == Id_p)&&(0 == Id_edp)&&(0 == Id_dp))//OTA,block read/write
	{
		rtn = 0;//valid
	}
	else
	{
		rtn = 1;
	}
return rtn;
}

/*
重要
Can Beta多字节 读取，  接收 的报文存储的 全局变量指针

找到该数据类型在本地缓存中的位置,指针ptr指向缓存位置



*node:输入，操作结构体
devId: CAN Rx ID源地址

type:

**ptr:  输出，CAN全局变量指针

*maxlen:输出
**cmd:输出



*/

void CanLookupTypePosition(uint8_t Writeflag, node_info_t *node, uint8_t devId, uint8_t type, uint8_t **ptr, uint32_t *maxlen, rw_cmd_struct **cmd)
{
    *ptr = NULL;
    *maxlen = 0;
    *cmd = NULL;

    uint8_t index =0;//	0~x:CAN 数组结构体索引， [x][y]的y

	index = CanDevIdCheck(node, devId);//正常情况下 index应始终是1
    if (index == 0xFF) {
        return ;
    }


    switch (type)
    {
        /* iot data type */
//        case 0x00: *ptr = (uint8_t *)&node->iot_info->iot_base_cantyp_0x00;   *maxlen = sizeof(iot_base_struct);   *cmd = &node->iot_info->iot_base_cantyp_0x00.rw_cmd;   break;
//        case 0x01: *ptr = (uint8_t *)&node->iot_info->iot_config_cantyp_0x01; *maxlen = sizeof(iot_config_struct); *cmd = &node->iot_info->iot_config_cantyp_0x01.rw_cmd; break;
//        case 0x02: *ptr = (uint8_t *)&node->iot_info->iot_about_cantyp_0x02;  *maxlen = sizeof(iot_about_struct);  *cmd = &node->iot_info->iot_about_cantyp_0x02.rw_cmd;  break;
//        case 0x03: *ptr = (uint8_t *)&node->iot_info->iot_wifi_cantyp_0x03;   *maxlen = sizeof(iot_wifi_struct);   *cmd = &node->iot_info->iot_wifi_cantyp_0x03.rw_cmd;   break;
//        case 0x04: *ptr = (uint8_t *)&node->iot_info->iot_ble_cantyp_0x04;    *maxlen = sizeof(iot_ble_struct);    *cmd = &node->iot_info->iot_ble_cantyp_0x04.rw_cmd;    break;
//        case 0x05: *ptr = (uint8_t *)&node->iot_info->iot_4g_cantyp_0x05;     *maxlen = sizeof(iot_4g_struct);     *cmd = &node->iot_info->iot_4g_cantyp_0x05.rw_cmd;     break;
//        case 0x06: *ptr = (uint8_t *)&node->iot_info->iot_local_cantyp_0x06;  *maxlen = sizeof(iot_local_struct);  *cmd = &node->iot_info->iot_local_cantyp_0x06.rw_cmd;  break;

//iot data type
		case 0x01:
			//*ptr = (uint8_t *)&node->iot_info->iot_can_11000;	*maxlen = sizeof(iot_11000_can_struct);	 *cmd = &(*node->iot_info).iot_can_11000.rw_cmd;
//        ESP_LOGE(TAG, "CanLookupTypePosition   type:0x%x, *maxlen=:%ld, sizeof(iot_11000_can_struct)=:%d, sizeof(rw_cmd_struct)=:%d",  type, *maxlen, sizeof(iot_11000_can_struct), sizeof(rw_cmd_struct));
		{
			if((devId >= IOT_CAN_ADDR) && (devId < (IOT_CAN_ADDR + (IOT_MAX_NUM*DEV_MAIN_NODE_MAX)) ) )
			{
				ESP_LOGI(TAG,"0X01 devId:%d,self:%d",devId,reals.Addr_can_self+IOT_CAN_ADDR);
				if(Writeflag==2)//作为从机被读取IOT板信息，需要传输自身设备信息
				{
					ESP_LOGI(TAG,"CanLookupTypePosition 0X01 2 ");
					*ptr = (uint8_t *)&Inv_can[reals.Addr_can_self].iot_data[0].mod_reg11000_IOT_info;	*maxlen = sizeof(iot_can_node_struct_reg11000);  *cmd = &Inv_can[reals.Addr_can_self].iot_data[0].mod_reg11000_IOT_info.rw_cmd;
				}else //读取到其他从机的IOT板信息，需要放置到从机源地址对应的数据结构
				{
					ESP_LOGI(TAG,"CanLookupTypePosition 0X01 1");
					*ptr = (uint8_t *)&node->iot_info->iot_can_11000;	*maxlen = sizeof(iot_11000_can_struct);	 *cmd = &(*node->iot_info).iot_can_11000.rw_cmd;
				}
			}else if((devId >= D400S_CAN_ADDR) && (devId < (D400S_CAN_ADDR + (D400S_MAX_NUM*DEV_MAIN_NODE_MAX)) ) )
			{
				*ptr = (uint8_t *)&(*node->d400s_s_info)[index].iot_can_11000;	*maxlen = sizeof(iot_11000_can_struct);	 *cmd = &(*node->d400s_s_info)[index].iot_can_11000.rw_cmd;
			}else if((devId >= ATS_CAN_ADDR) && (devId < (ATS_CAN_ADDR + (ATS_MAX_NUM*DEV_MAIN_NODE_MAX)) ) )
			{
				if(Writeflag==2)//作为从机被读取IOT板信息，需要传输自身设备信息
				{
					ESP_LOGI(TAG,"ATS CanLookupTypePosition 0X01 2 ");
					*ptr = (uint8_t *)&Inv_can[reals.Addr_can_self].iot_data[0].mod_reg11000_IOT_info;	*maxlen = sizeof(iot_11000_can_struct);  *cmd = &Inv_can[reals.Addr_can_self].iot_data[0].mod_reg11000_IOT_info.rw_cmd;
				}
			}
		}
		break;
		case IOT_TYPE_SET_02H: //set
//			*ptr = (uint8_t *)&node->iot_info->iot_can_12000; 	*maxlen = sizeof(iot_12000_can_struct);  *cmd = &(*node->iot_info).iot_can_12000.rw_cmd;
			if(1 == Writeflag)//write
			{

				*ptr = (uint8_t *)&Inv_can_WR.mod_IOT_set.mod_reg12000_IOT_set; *maxlen = sizeof(iot_can_node_struct_reg12000);  *cmd = &Inv_can_WR.mod_IOT_set.mod_reg12000_IOT_set.rw_cmd;
			}
			else//read
			{
				*ptr = (uint8_t *)&Inv_can[reals.Addr_can_self].iot_data[0].mod_reg12000_IOT_set;	*maxlen = sizeof(iot_can_node_struct_reg12000);  *cmd = &Inv_can[reals.Addr_can_self].iot_data[0].mod_reg12000_IOT_set.rw_cmd;
			}

    //    ESP_LOGE(TAG, "CanLookupTypePosition   type:0x%x, *maxlen=:%ld, sizeof(iot_12000_can_struct)=:%d, sizeof(rw_cmd_struct)=:%d",  type, *maxlen, sizeof(iot_12000_can_struct), sizeof(rw_cmd_struct));
		break;

        /* inv data type */
        case 0x10:
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_announce;   *maxlen = sizeof(inv_announce_struct);   *cmd = &(*node->invs_info)[index].inv_announce.rw_cmd;


		break;
        case 0x11:
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_base;       *maxlen = sizeof(inv_base_struct);       *cmd = &(*node->invs_info)[index].inv_base.rw_cmd;


		break;
        case 0x12:
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_line;       *maxlen = sizeof(inv_line_struct);       *cmd = &(*node->invs_info)[index].inv_line.rw_cmd;


		break;
        case 0x13:
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_about;      *maxlen = sizeof(inv_about_struct);      *cmd = &(*node->invs_info)[index].inv_about.rw_cmd;


		break;
        case 0x14:
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_pv;         *maxlen = sizeof(inv_pv_struct);         *cmd = &(*node->invs_info)[index].inv_pv.rw_cmd;


		break;
        case 0x15:
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_grid;       *maxlen = sizeof(inv_grid_struct);       *cmd = &(*node->invs_info)[index].inv_grid.rw_cmd;


		break;
        case 0x16:
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_load;       *maxlen = sizeof(inv_load_struct);       *cmd = &(*node->invs_info)[index].inv_load.rw_cmd;


		break;
        case 0x17:
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_data;       *maxlen = sizeof(inv_data_struct);       *cmd = &(*node->invs_info)[index].inv_data.rw_cmd;


		break;
        case 0x18:
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_meter;      *maxlen = sizeof(inv_meter_struct);      *cmd = &(*node->invs_info)[index].inv_meter.rw_cmd;


		break;
        case 0x19:
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_gen;        *maxlen = sizeof(inv_gen_struct);        *cmd = &(*node->invs_info)[index].inv_gen.rw_cmd;


		break;
        case INV_TYPE_CONFIG00_1AH: //set
			if(1 == Writeflag)//write
			{
				ESP_LOGE(TAG,"!!!!!!!!!!!write in INV_TYPE_CONFIG00_1AH");
				*ptr = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00; *maxlen = sizeof(inv_set00_struct);  *cmd = &Inv_can_WR.bk_inv_dev_set.inv_set00.rw_cmd;
				ESP_LOGI(TAG,"0X1A clear_all:%d",Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl.bit.clear_all);
			}
			else//read
			{
				*ptr = (uint8_t *)&(*node->invs_info)[index].inv_set00; 	 *maxlen = sizeof(inv_set00_struct);	  *cmd = &(*node->invs_info)[index].inv_set00.rw_cmd;
			}

		break;
        case INV_TYPE_CONFIG01_1BH: //set
			if(1 == Writeflag)//write
			{

				*ptr = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01; *maxlen = sizeof(inv_set01_struct);  *cmd = &Inv_can_WR.bk_inv_dev_set.inv_set01.rw_cmd;
			}
			else//read
			{
				*ptr = (uint8_t *)&(*node->invs_info)[index].inv_set01; 	 *maxlen = sizeof(inv_set01_struct);	  *cmd = &(*node->invs_info)[index].inv_set01.rw_cmd;
			}

		break;
        case INV_TYPE_CONFIG02_1CH: //set
			if(1 == Writeflag)//write
			{

			*ptr = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set02; *maxlen = sizeof(inv_set02_struct);  *cmd = &Inv_can_WR.bk_inv_dev_set.inv_set02.rw_cmd;
			}
			else//read
			{
				*ptr = (uint8_t *)&(*node->invs_info)[index].inv_set02; 	 *maxlen = sizeof(inv_set02_struct);	  *cmd = &(*node->invs_info)[index].inv_set02.rw_cmd;
			}

		break;
        case INV_TYPE_CONFIG03_1DH: //set
			if(1 == Writeflag)//write
			{

				*ptr = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set03; *maxlen = sizeof(inv_set03_struct);  *cmd = &Inv_can_WR.bk_inv_dev_set.inv_set03.rw_cmd;
			}
			else//read
			{
				*ptr = (uint8_t *)&(*node->invs_info)[index].inv_set03; 	 *maxlen = sizeof(inv_set03_struct);	  *cmd = &(*node->invs_info)[index].inv_set03.rw_cmd;
			}

		break;
        case 0x20:
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_log;        *maxlen = sizeof(inv_log_struct);        *cmd = &(*node->invs_info)[index].inv_log.rw_cmd;


		break;
        case 0x21:
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_all_energy; *maxlen = sizeof(inv_all_energy_struct); *cmd = &(*node->invs_info)[index].inv_all_energy.rw_cmd;


		break;
        case 0x22:
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_energy;     *maxlen = sizeof(inv_energy_struct);     *cmd = &(*node->invs_info)[index].inv_energy.rw_cmd;


		break;
        case INV_TYPE_WIFI_23H: //福达
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_wifi;       *maxlen = sizeof(inv_wifi_struct);       *cmd = &(*node->invs_info)[index].inv_wifi.rw_cmd;

		break;
		case 0x24: //福达
			*ptr = (uint8_t *)&(*node->invs_info)[index].wifi_report;    *maxlen = sizeof(wifi_report_struct);    *cmd = &(*node->invs_info)[index].wifi_report.rw_cmd;


		break;
		case 0x25: //福达
			if(1 == Writeflag)//write
			{

				*ptr = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_wifi; *maxlen = sizeof(wifi_param_struct);  *cmd = &Inv_can_WR.bk_inv_dev_set.inv_wifi.rw_cmd;
			}
			else//read
			{
				*ptr = (uint8_t *)&(*node->invs_info)[index].wifi_param;	 *maxlen = sizeof(wifi_param_struct);	  *cmd = &(*node->invs_info)[index].wifi_param.rw_cmd;
			}

		break;

		case INV_TYPE_CERT_27H: //set
			if(1 == Writeflag)//write
			{

				*ptr = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.auth_param; *maxlen = sizeof(auth_struct);  *cmd = &Inv_can_WR.bk_inv_dev_set.auth_param.rw_cmd;
			}
			else//read
			{
				*ptr = (uint8_t *)&(*node->invs_info)[index].auth_param;	 *maxlen = sizeof(auth_struct); 	*cmd = &(*node->invs_info)[index].auth_param.rw_cmd;
			}

		break;
		case MODULE_TYPE_DC_AC_HUB_SET_40H: //set tbd
			if(1 == Writeflag)//write
			{

				*ptr = (uint8_t *)&Inv_can_mix_WR.dc_ac_hub_setting; *maxlen = sizeof(auth_struct);  *cmd = &Inv_can_mix_WR.dc_ac_hub_setting.rw_cmd;
			}
			else//read
			{
				*ptr = (uint8_t *)&(*node->achubs_info)[index].dc_ac_hub_setting;	 *maxlen = sizeof(auth_struct); 	*cmd = &(*node->achubs_info)[index].dc_ac_hub_setting.rw_cmd;
			}

		break;


		case 0x41:
			*ptr = (uint8_t *)&(*node->dchubs_info)[index].dc_hub_info;     *maxlen = sizeof(dc_hub_info_struct);     *cmd = &(*node->dchubs_info)[index].dc_hub_info.rw_cmd;


		break;
		case 0x42:
			*ptr = (uint8_t *)&(*node->achubs_info)[index].ac_hub_info;     *maxlen = sizeof(ac_hub_info_struct);     *cmd = &(*node->achubs_info)[index].ac_hub_info.rw_cmd;


		break;
		case INV_TYPE_DCDC_48H:
		{
			*ptr = (uint8_t *)&(*node->d400s_s_info)[index].d400s_common_info;     *maxlen = sizeof(d400s_common_info_struct);     *cmd = &(*node->d400s_s_info)[index].d400s_common_info.rw_cmd;
		}
		break;

		case INV_TYPE_DCDC_49H:
		{
			*ptr = (uint8_t *)&(*node->d400s_s_info)[index].d400s_charger_set;     *maxlen = sizeof(d400s_charger_set_struct);     *cmd = &(*node->d400s_s_info)[index].d400s_charger_set.rw_cmd;
		}
		break;
        /* pack data type */
        case 0x50:
			*ptr = (uint8_t *)&(*node->packs_info)[index].pack_announce; *maxlen = sizeof(pack_announce_struct);  *cmd = &(*node->packs_info)[index].pack_announce.rw_cmd;


		break;
        case 0x51:
			*ptr = (uint8_t *)&(*node->packs_info)[index].pack_base;     *maxlen = sizeof(pack_base_struct);   	  *cmd = &(*node->packs_info)[index].pack_base.rw_cmd;


		break;
        case 0x52:
			*ptr = (uint8_t *)&(*node->packs_info)[index].pack_extend;   *maxlen = sizeof(pack_extend_struct); 	  *cmd = &(*node->packs_info)[index].pack_extend.rw_cmd;


		break;
        case 0x54:
			*ptr = (uint8_t *)&(*node->packs_info)[index].pack_about;    *maxlen = sizeof(pack_about_struct);  	  *cmd = &(*node->packs_info)[index].pack_about.rw_cmd;


		break;
        case PACK_TYPE_CONFIG_55H: //set
			if(1 == Writeflag)//write
			{

				*ptr = (uint8_t *)&Inv_can_WR.bk_pack_dev_set.pack_config; *maxlen = sizeof(pack_config_struct);  *cmd = &Inv_can_WR.bk_pack_dev_set.pack_config.rw_cmd;
			}
			else//read
			{
				*ptr = (uint8_t *)&(*node->packs_info)[index].pack_config;	 *maxlen = sizeof(pack_config_struct);	  *cmd = &(*node->packs_info)[index].pack_config.rw_cmd;
			}

		break;
        case 0x56:
			*ptr = (uint8_t *)&(*node->packs_info)[index].pack_debug1.payload[0]; *maxlen = sizeof(pack_debug1_struct)-4; *cmd = &(*node->packs_info)[index].pack_debug1.rw_cmd;


		break;

        /* file data type */
        case 0xFA:
			*ptr = (uint8_t *)&Inv_can_WR.mod_IOT_set.iot_can_masetr_file; 	*maxlen = sizeof(iot_file_can_masetr_struct);  *cmd = &Inv_can_WR.mod_IOT_set.iot_can_masetr_file.rw_cmd;

		break;
        case 0xFB:
			*ptr = (uint8_t *)&Inv_can_WR.mod_IOT_set.iot_can_slave_file; 	*maxlen = sizeof(iot_file_can_slave_struct);   *cmd = &Inv_can_WR.mod_IOT_set.iot_can_slave_file.rw_cmd;

		break;

        /* iot factory type:生产CAN标定 */

        case 0xFF: *ptr = (uint8_t *)&Inv_can_WR.factory;  *maxlen = sizeof(iot_factory_struct); *cmd = &Inv_can_WR.factory.rw_cmd;

		break;
        default: ESP_LOGE(TAG, "unkown type(0x%x) node(%d)", type, devId);

		return ;
    }

    if (*maxlen >= sizeof(rw_cmd_struct))
        *maxlen -= sizeof(rw_cmd_struct);
}



/*
重要
Can Beta多字节 读取，  接收 的报文存储的 全局变量指针

找到该数据类型在本地缓存中的位置,指针ptr指向缓存位置

IOT自身通过多字节读取和写入的差别

Writeflag:1-写入本机IOT；0-读取本机IOT

devId: CAN Rx ID源地址

type:

**ptr:  输出，CAN全局变量指针

*maxlen:输出
**cmd:输出



*/

void CanLookupTypePosition_Iot_self(uint8_t Writeflag,  uint8_t type, uint8_t **ptr, uint32_t *maxlen, rw_cmd_struct **cmd)
{

    *ptr = NULL;
    *maxlen = 0;
    *cmd = NULL;




    switch (type)
    {
//iot data type
		case 0x01:
			if(1 == Writeflag)//write
			{

			}
			else//read
			{
				*ptr = (uint8_t *)&Inv_can[reals.Addr_can_self].iot_data[0].mod_reg11000_IOT_info;	*maxlen = sizeof(iot_can_node_struct_reg11000);  *cmd = &Inv_can[reals.Addr_can_self].iot_data[0].mod_reg11000_IOT_info.rw_cmd;
			}

//        ESP_LOGE(TAG, "CanLookupTypePosition_Iot_self   type:0x%x, *maxlen=:%ld, sizeof(iot_11000_can_struct)=:%d, sizeof(rw_cmd_struct)=:%d",  type, *maxlen, sizeof(iot_11000_can_struct), sizeof(rw_cmd_struct));

		break;
		case 0x02:
			if(1 == Writeflag)//write
			{

				*ptr = (uint8_t *)&Inv_can_WR.mod_IOT_set.mod_reg12000_IOT_set;	*maxlen = sizeof(iot_can_node_struct_reg12000);  *cmd = &Inv_can_WR.mod_IOT_set.mod_reg12000_IOT_set.rw_cmd;
			}
			else//read
			{
				*ptr = (uint8_t *)&Inv_can[reals.Addr_can_self].iot_data[0].mod_reg12000_IOT_set;	*maxlen = sizeof(iot_can_node_struct_reg12000);  *cmd = &Inv_can[reals.Addr_can_self].iot_data[0].mod_reg12000_IOT_set.rw_cmd;
			}

//        ESP_LOGE(TAG, "CanLookupTypePosition_Iot_self   type:0x%x, *maxlen=:%ld, sizeof(iot_12000_can_struct)=:%d, sizeof(rw_cmd_struct)=:%d",  type, *maxlen, sizeof(iot_12000_can_struct), sizeof(rw_cmd_struct));
		break;
//		case 0x1A:
//			if(1 == Writeflag)//write
//			{
//				*ptr = (uint8_t *)&Inv_can[reals.online_X_inv_index].inv_data[0].inv_set00;	*maxlen = sizeof(inv_set00_struct);  *cmd = &Inv_can[reals.online_X_inv_index].inv_data[0].inv_set00.rw_cmd;
//			}
//			else//read
//			{
//				*ptr = (uint8_t *)&Inv_can[reals.Addr_can_self].inv_data[0].inv_set00;	*maxlen = sizeof(inv_set00_struct);  *cmd = &Inv_can[reals.Addr_can_self].inv_data[0].inv_set00.rw_cmd;
//			}
//		break;
		case UDT_QUERY_CMD:
			if(1 == Writeflag)//write
			{
        		*ptr = (uint8_t *)&Inv_can_WR.mod_IOT_set.iot_can_masetr_file; 	*maxlen = sizeof(iot_file_can_masetr_struct);  *cmd = &Inv_can_WR.mod_IOT_set.iot_can_masetr_file.rw_cmd;
			}
			else//read
			{
				// *ptr = (uint8_t *)&Inv_can[reals.Addr_can_self].inv_data[0].inv_set00;	*maxlen = sizeof(inv_set00_struct);  *cmd = &Inv_can[reals.Addr_can_self].inv_data[0].inv_set00.rw_cmd;
			}
		break;
		case UDT_RESP_CMD:
			if(1 == Writeflag)//write
			{
				*ptr = (uint8_t *)&Inv_can_WR.mod_IOT_set.iot_can_slave_file; 	*maxlen = sizeof(iot_file_can_slave_struct);  *cmd = &Inv_can_WR.mod_IOT_set.iot_can_slave_file.rw_cmd;
			}
			else//read
			{
				// *ptr = (uint8_t *)&Inv_can[reals.Addr_can_self].inv_data[0].inv_set00;	*maxlen = sizeof(inv_set00_struct);  *cmd = &Inv_can[reals.Addr_can_self].inv_data[0].inv_set00.rw_cmd;
			}
		break;
        case 0xFF: *ptr = (uint8_t *)&Inv_can_WR.factory;  *maxlen = sizeof(iot_factory_struct); *cmd = &Inv_can_WR.factory.rw_cmd; break;
        default: ESP_LOGE(TAG, "unkown type(0x%x) ", type); return ;
    }

    if (*maxlen >= sizeof(rw_cmd_struct))
        *maxlen -= sizeof(rw_cmd_struct);
}


/*
多字接操作，命令报文

初始化多字节操作的结构体和指针环境，包括：
master read
master write
as slave ,by master read

CanId: CAN Rx ID
devId: CAN Rx ID源地址


input: *p_cmd:将 CAN  RX data结构为结构体，解析
isWrite:1-自己做主，多字节读其他设备；自己做从，被其他主设备写
		0-自己做从，被其他主设备读取

*/
uint8_t CanVerifyCmd(node_info_t *node, uint32_t CanId, uint8_t isWrite, uint8_t devId, CmdStruct *p_cmd, dev_data *payload)
{
	uint8_t Writeflag=0;

    IdStruct RxcanId ;
	RxcanId.all = CanId;

	if (!p_cmd) return ERR_TYPE;

	uint8_t type = p_cmd->type;
	uint16_t offset = p_cmd->start;
	uint16_t len = p_cmd->total;
	uint16_t crc16 = p_cmd->crc16;
    uint32_t maxlen = 0;
    uint8_t *ptr = NULL;//从CAN全局变量表格获得的待操作全局变量指针
    rw_cmd_struct *cmd = NULL;

    if (payload) {
        payload->len = 0;
        payload->pdata = NULL;
    }

	/* 找到该数据类型在本地缓存中的位置,指针ptr指向缓存位置 */
	//ESP_LOGI(TAG,"CanVerifyCmd ALL:0x%lx", RxcanId.all);
	if(WRIET_DATA_START == RxcanId.bit.funcode)//
	{
//		CanLookupTypePosition_Iot_self(0, type, &ptr, &maxlen, &cmd);
		Writeflag =1;

	}
	else if(READ_DATA_CMD==RxcanId.bit.funcode)
	{
		Writeflag =2; //从机IOT接收到主机的读取指令
	}
	else
	{
		Writeflag =0;
	}

	CanLookupTypePosition(Writeflag, node, devId, type, &ptr, &maxlen, &cmd);

    if (!cmd || !ptr || !maxlen) {
        return ERR_TYPE;
    }

    if (maxlen == 0) {
        return ERR_RANGE;
    }

    if ((maxlen - offset) < len)
	{ // 需要操作的长度超过范围

        ESP_LOGE(TAG, "devId:0x%x type:0x%x offset:%d too long(max=%ld, recv=%d)", devId, type, offset, maxlen, len);
        if (isWrite == 1) {
            CanCmdUpdate(cmd, devId,type, 0, 0, 0); // 擦除记录
        }
        return ERR_RANGE;
    }

    if (isWrite == 1)
	{ /* 数据写入本地 */

        CanCmdUpdate(cmd, devId,type, 0, 0, 0); // 消除命令记录

        if (cmd->temp_buffer != NULL) { /* 数据开始,可能上次开辟的空间不足,需要重新开辟空间 */
            free(cmd->temp_buffer);
            cmd->temp_buffer = NULL;
        }

		if((type == reals.Can_beta_block_type)
			&&(READ_RESP_DATA_START == ((CanId>>16)&0xFF)))//can id fun
		{
			if(0 == len)//最后一帧rx
			{
				reals.Can_beta_block_send_timeout_Cnt =CAN_BLOCK_RX_TIME_OUT_FAST;//tbd

			}
			else
			{
				reals.Can_beta_block_send_timeout_Cnt =CAN_BLOCK_RX_TIME_OUT_SLOW;//tbd
			}
			//ESP_LOGE(TAG, "Can_beta_block_send_timeout_Cnt HUANGJI AAA:%d", reals.Can_beta_block_send_timeout_Cnt);
		}


        if ((cmd->temp_buffer == NULL) && (len != 0) )
		{
            #ifdef STATE_COUNT
            cmd->recv_cnt++;
            #endif
			/* 为写入的数据分配缓存 */
            cmd->temp_buffer = heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
            if (cmd->temp_buffer == NULL) {
                ESP_LOGE(TAG, "can type: 0x%x memony malloc failed %dbytes", type, len);
                return ERR_MEMONY;
            }
            CanCmdUpdate(cmd, devId,type, offset, len, crc16); // 校验,偏移量,长度,设备ID
        }
    }
	else //read
	{
        if (payload == NULL)
		{
            return ERR_MEMONY;
        }

ESP_LOGE(TAG,"windy test  typemaxlen= 0x%x, maxlen= %ld , offset== %d ",type,maxlen,offset);
        payload->len = (len) ? (len) : (maxlen - offset);//如果对方查询指定长度，就取指定长度，否则 自己计算最大长度，全部上传
        uint8_t *tmp_data = heap_caps_malloc(payload->len, MALLOC_CAP_SPIRAM); //
        if (tmp_data) {
            payload->pdata = tmp_data;
            memcpy(payload->pdata, (ptr + offset), payload->len);
        }
    }
    return ERR_NONE;
}

/*
 * 0x1A 写广播：Inv_can_WR -> Inv_can[].inv_set00，并触发汇总寄存器刷新
 * return: 1 已同步开关类字段(2011/2012/2093)，需立即汇总
 */
static uint8_t can_inv_set00_write_bcast_sync(node_info_t *node, uint8_t devId, uint16_t write_offset, uint16_t data_len)
{
	inv_set00_struct *src = &Inv_can_WR.bk_inv_dev_set.inv_set00;
	uint8_t inv_index = CanDevIdCheck(node, devId);
	inv_set00_struct *dst;
	inv_set00_struct *sum_dst;

	if (inv_index == 0xFF || node == NULL || node->invs_info == NULL) {
		return 0;
	}

	dst = &(*node->invs_info)[inv_index].inv_set00;
	sum_dst = &(*node->invs_info)[INV_MAX_NUM].inv_set00;

	if (write_offset == offsetof(inv_set00_struct, ctrl_ac) && data_len >= sizeof(dst->ctrl_ac)) {
		dst->ctrl_ac = src->ctrl_ac;
		sum_dst->ctrl_ac = src->ctrl_ac;
		return 1;
	}
	if (write_offset == offsetof(inv_set00_struct, ctrl_dc) && data_len >= sizeof(dst->ctrl_dc)) {
		dst->ctrl_dc = src->ctrl_dc;
		sum_dst->ctrl_dc = src->ctrl_dc;
		return 1;
	}
	if (write_offset == offsetof(inv_set00_struct, set_AC_branch) && data_len >= sizeof(dst->set_AC_branch)) {
		dst->set_AC_branch = src->set_AC_branch;
		sum_dst->set_AC_branch = src->set_AC_branch;
		return 1;
	}
	if (write_offset == offsetof(inv_set00_struct, self_config) && data_len >= sizeof(dst->self_config)) {
		memcpy((uint8_t *)&dst->self_config, (uint8_t *)&src->self_config, sizeof(dst->self_config));
		memcpy((uint8_t *)&sum_dst->self_config, (uint8_t *)&src->self_config, sizeof(src->self_config));
		return 0;
	}
	if (write_offset == offsetof(inv_set00_struct, ctrl_chg_mode) && data_len >= sizeof(dst->ctrl_chg_mode)) {
		dst->ctrl_chg_mode = src->ctrl_chg_mode;
		sum_dst->ctrl_chg_mode = src->ctrl_chg_mode;
		return 0;
	}
	if (write_offset == offsetof(inv_set00_struct, remoteSet) && data_len >= sizeof(dst->remoteSet)) {
		memcpy((uint8_t *)&dst->remoteSet, (uint8_t *)&src->remoteSet, sizeof(dst->remoteSet));
		memcpy((uint8_t *)&sum_dst->remoteSet, (uint8_t *)&src->remoteSet, sizeof(src->remoteSet));
		return 0;
	}
	if (write_offset == offsetof(inv_set00_struct, LevelSwitch) && data_len >= sizeof(dst->LevelSwitch)) {
		memcpy((uint8_t *)&dst->LevelSwitch, (uint8_t *)&src->LevelSwitch, sizeof(dst->LevelSwitch));
		memcpy((uint8_t *)&sum_dst->LevelSwitch, (uint8_t *)&src->LevelSwitch, sizeof(src->LevelSwitch));
		return 0;
	}
	return 0;
}

/*
CAN多字节读取的 接收 解析，赋值到CAN 全局变量结构体
devId： can ID rx源地址

*/
uint8_t CanVerifyData(node_info_t *node, uint32_t CanId, uint8_t isWrite, uint8_t devId, uint8_t type, uint8_t seq, uint8_t len, const uint8_t *payload) {
   // isWrite = 1,表示数据是给到到本机存储
	uint8_t Writeflag=0;

    IdStruct RxcanId ;
	RxcanId.all = CanId;

    uint32_t maxlen = 0;
    uint8_t *ptr = NULL;//从CAN全局变量表格获得的待操作全局变量指针
    rw_cmd_struct *cmd = NULL;

    if (!isWrite || !payload) { // 数据存入本地
        return ERR_TYPE;
    }

	/* 找到该数据类型在本地缓存中的位置,指针ptr指向缓存位置 */

	//ESP_LOGI(TAG,"CanVerifyData ALL:0x%lx", RxcanId.all);
	if(WRITE_DATA == RxcanId.bit.funcode)//write iot self
	{
//		CanLookupTypePosition_Iot_self(1, type, &ptr, &maxlen, &cmd);
		Writeflag =1;

	}
	else
	{
		Writeflag =0;

	}
	CanLookupTypePosition(Writeflag, node, devId, type, &ptr, &maxlen, &cmd);

    if (!cmd || !ptr) {
        return ERR_TYPE;
    }

    if (cmd->devId != devId) { // 设备ID号不同
    ESP_LOGE(TAG,"cmd->devId != devId , cmd->devId : %d, devId : %d", cmd->devId, devId);
	ESP_LOGE(TAG,"CanId: 0x%lX",CanId);
        return ERR_WAIT;
    }

    if (cmd->can_type != type) { // 多字节操作，要关联can type,不同的丢弃
    ESP_LOGE(TAG,"cmd->can_type != type , cmd->can_type : %d, type : %d", cmd->can_type, type);
        return ERR_WAIT;
    }


    if (cmd->write_next_seq != seq)
	{ // 等待的帧序号不相同
    ESP_LOGE(TAG,"test (cmd->write_next_seq != seq), CanId: 0x%lX, cmd->write_next_seq == %d, seq == %d, min == %d, sec == %d",CanId,cmd->write_next_seq,seq,reals.rtc_time.min,reals.rtc_time.sec);
        return ERR_WAIT;
    }

    if (cmd->temp_buffer == NULL) { // 无法缓存数据
        return ERR_MEMONY;
    }


	if((type == reals.Can_beta_block_type)
		&&(READ_RESP_DATA == ((CanId>>16)&0xFF)))//can id fun
	{
		if((cmd->write_remain_len == len)//最后一帧rx
		||(cmd->write_remain_len <= FRAME_DATA_BYTES))
		{
			reals.Can_beta_block_send_timeout_Cnt =CAN_BLOCK_RX_TIME_OUT_FAST;//tbd

		}
		else
		{
			reals.Can_beta_block_send_timeout_Cnt =CAN_BLOCK_RX_TIME_OUT_SLOW;//tbd
		}
		//ESP_LOGE(TAG, "Can_beta_block_send_timeout_Cnt HUANGJI BBB:%d", reals.Can_beta_block_send_timeout_Cnt);
	}


    if ((cmd->write_remain_len >= len) && (len <= FRAME_DATA_BYTES))
	{  // 每个CAN帧最多6字节数据
        memcpy((cmd->temp_buffer + (seq * FRAME_DATA_BYTES)), payload, len);
        cmd->write_next_seq++;
        cmd->write_remain_len -= len;

		//ESP_LOGI(TAG, "CanVerifyData : devId: 0x%02x type: 0x%02x ,write_remain_len: %d len: %d ", devId, type, cmd->write_remain_len, len);

        if (cmd->write_remain_len != 0) {
            return ERR_WAIT; /* 数据未接收完整 */
        }
        cmd->write_next_seq = 0xFF; /* 接收完成 */

        uint16_t data_len = ((uint16_t)seq * FRAME_DATA_BYTES) + len;
        uint16_t crc16 = calcu_crc16(cmd->temp_buffer, data_len);

		/* 数据接收完成校验成功后写入到本地缓存 */
        if (cmd->write_crc16 == crc16)
		{
            cmd->crc_valid = 1; /* crc正确 */
            if ((cmd->write_offset + data_len) <= maxlen)
			{
#if CAN_DEBUG_LOG_0X11_RX
				if (type == 0x11) {
					can_debug_log_inv_base_0x11_before_copy(devId, cmd->write_offset, cmd->temp_buffer, data_len);
				}
#endif
#if CAN_DEBUG_LOG_0X16_RX
				if (type == INV_TYPE_LOAD_16H) {
					can_debug_log_inv_load_0x16_before_copy(devId, cmd->write_offset, cmd->temp_buffer, data_len);
				}
#endif
#if CAN_DEBUG_LOG_0X15_RX
				if (type == INV_TYPE_GRID_15H) {
					can_debug_log_inv_grid_0x15_before_copy(devId, cmd->write_offset, cmd->temp_buffer, data_len);
				}
#endif
                memcpy((ptr + cmd->write_offset), cmd->temp_buffer, data_len);
				switch(type)
				{
					case IOT_TYPE_SET_02H:
					{
						if(WRITE_DATA == RxcanId.bit.funcode)
						{
							reals.ModbusCmdFlag.sBit.iot_can_set_mix =1;
							Can_iot_callback_set_type_0x2(cmd->write_offset, data_len);
						}
					}
					break;
					case INV_TYPE_ABOUT_13H:
					{
						CAN_DEV_SN_Get(devId);
					}
					break;
					case INV_TYPE_LOAD_16H:
					{
						ESP_LOGI(TAG,"is_invload_16_poll:%lu",is_invload_16_poll());
						if(!is_invload_16_poll())
						{
							can_data_poll_mask_set(CAN_ACK_INV_LOAD_16H,1);
						}
					}
					break;
					case INV_TYPE_DATA_17H:
					{
						ESP_LOGI(TAG,"is_invdata_17_poll:%lu",is_invdata_17_poll());
						if(!is_invdata_17_poll())
						{
							can_data_poll_mask_set(CAN_ACK_INV_DATA_17H,1);
						}
					}
					break;
					case INV_TYPE_CONFIG00_1AH:
					{
						uint16_t ac_btn_off = (uint16_t)offsetof(inv_set00_struct, AC_button_sign);
						if ((cmd->write_offset <= ac_btn_off)
							&& ((cmd->write_offset + data_len) > ac_btn_off)
							&& (((inv_set00_struct *)ptr)->AC_button_sign == 1)) {
							app_ac_button_sign_handle();
						}
						if(WRITE_DATA == RxcanId.bit.funcode)
						{
							uint8_t need_sum = can_inv_set00_write_bcast_sync(node, devId, cmd->write_offset, data_len);
							if (need_sum) {
								data_sum_inv_base_param();
							}
						}
					}
					break;
					case INV_TYPE_CONFIG01_1BH:
					{
						if(WRITE_DATA == RxcanId.bit.funcode)
						{
							ESP_LOGE(TAG,"INV_TYPE_CONFIG00_1AH cmd->write_offset: %d, data_len: %d", cmd->write_offset, data_len);
							if(cmd->write_offset == 0x70)
							{
								// 各个单逆变的
								memcpy((uint8_t *)&(*node->invs_info)[0].inv_set01.Func_Set,
									(uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01.Func_Set, sizeof(Inv_can_WR.bk_inv_dev_set.inv_set01.Func_Set));//AC开关
								// 逆变汇总的
								memcpy((uint8_t *)&(*node->invs_info)[INV_MAX_NUM].inv_set01.Func_Set,
									(uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01.Func_Set, sizeof(Inv_can_WR.bk_inv_dev_set.inv_set01.Func_Set));//AC开关
								ESP_LOGI(TAG,"000---Inv_can[0].Func_Set:%u,Inv_can[1].ctrl_ac:%u,Inv_can[2].ctrl_ac:%u,Inv_can[3].ctrl_ac:%u",Inv_can[0].inv_data[0].inv_set01.Func_Set.all,Inv_can[1].inv_data[0].inv_set01.Func_Set.all,Inv_can[2].inv_data[0].inv_set01.Func_Set.all,Inv_can[3].inv_data[0].inv_set01.Func_Set.all);
								ESP_LOGI(TAG,"111---Inv_can[0].Func_Set:%u,Inv_can[1].ctrl_ac:%u,Inv_can[2].ctrl_ac:%u,Inv_can[3].ctrl_ac:%u",Inv_can[0].inv_data[1].inv_set01.Func_Set.all,Inv_can[1].inv_data[1].inv_set01.Func_Set.all,Inv_can[2].inv_data[1].inv_set01.Func_Set.all,Inv_can[3].inv_data[1].inv_set01.Func_Set.all);
							}
							else if(cmd->write_offset == 0x6E)
							{
								// 各个单逆变的
								memcpy((uint8_t *)&(*node->invs_info)[0].inv_set01.SetGridMaxCurrent_in,
									(uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01.SetGridMaxCurrent_in, sizeof(Inv_can_WR.bk_inv_dev_set.inv_set01.Func_Set));//AC开关
								// 逆变汇总的
								memcpy((uint8_t *)&(*node->invs_info)[INV_MAX_NUM].inv_set01.SetGridMaxCurrent_in,
									(uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01.SetGridMaxCurrent_in, sizeof(Inv_can_WR.bk_inv_dev_set.inv_set01.Func_Set));//AC开关
								ESP_LOGI(TAG,"000---Inv_can[0].SetGridMaxCurrent_in:%u,Inv_can[1].ctrl_ac:%u,Inv_can[2].ctrl_ac:%u,Inv_can[3].ctrl_ac:%u",Inv_can[0].inv_data[0].inv_set01.Func_Set.all,Inv_can[1].inv_data[0].inv_set01.SetGridMaxCurrent_in,Inv_can[2].inv_data[0].inv_set01.SetGridMaxCurrent_in,Inv_can[3].inv_data[0].inv_set01.SetGridMaxCurrent_in);
								ESP_LOGI(TAG,"111---Inv_can[0].SetGridMaxCurrent_in:%u,Inv_can[1].ctrl_ac:%u,Inv_can[2].ctrl_ac:%u,Inv_can[3].ctrl_ac:%u",Inv_can[0].inv_data[1].inv_set01.Func_Set.all,Inv_can[1].inv_data[1].inv_set01.SetGridMaxCurrent_in,Inv_can[2].inv_data[1].inv_set01.SetGridMaxCurrent_in,Inv_can[3].inv_data[1].inv_set01.SetGridMaxCurrent_in);
							}
						}
					}
					break;
					case MODULE_TYPE_DC_AC_HUB_SET_40H:
					{
						ESP_LOGE(TAG,"MODULE_TYPE_DC_AC_HUB_SET_40H cmd->write_offset: %d, data_len: %d", cmd->write_offset, data_len);
						if(WRITE_DATA == RxcanId.bit.funcode)
						{
							if(cmd->write_offset == 0x02)
							{
								// ACHUB只有总的
								(*node->achubs_info)[0].dc_ac_hub_setting.ac_hug_setting.bit.ac_switch = Inv_can_mix_WR.dc_ac_hub_setting.ac_hug_setting.bit.ac_switch;//HA1开关
							}
						}
					}
					break;
					case 0x42:
					{
						ESP_LOGW(TAG, "srcaddr:0x%x, 0x42 temp_buffer:0x%x-0x%x-0x%x-0x%x-0x%x-0x%x",RxcanId.bit.src, cmd->temp_buffer[63], cmd->temp_buffer[64], cmd->temp_buffer[65], cmd->temp_buffer[66], cmd->temp_buffer[67], cmd->temp_buffer[68]);
						ESP_LOG_BUFFER_HEX(TAG, &cmd->temp_buffer[0], 73);
						ESP_LOGW(TAG,"INV_CAN 0X42 Get:%d",(unsigned int)Inv_can_mix.ac_hub_data[0].ac_hub_info.SoftwareVersion);
						ESP_LOG_BUFFER_HEX(TAG, (char *)(&Inv_can_mix.ac_hub_data[0].ac_hub_info),73);
						ESP_LOGI(TAG,"is_achub_42_poll:%lu write_offset:%u,data_len:%u",is_achub_42_poll(),cmd->write_offset,data_len);
						ESP_LOGI(TAG,"0x42 Power_grid:%u -%u -%u",Inv_can_mix.ac_hub_data[0].ac_hub_info.Power_grid[0],Inv_can_mix.ac_hub_data[0].ac_hub_info.Power_grid[1],Inv_can_mix.ac_hub_data[0].ac_hub_info.Power_grid[2] );
						if(!is_achub_42_poll())
						{
							can_data_poll_mask_set(CAN_ACK_AC_HUB_INFO_42H,1);
						}
					}
					break;
					case 0x54:
					{
						ESP_LOGI(TAG,"0X54 rev %x-%x-%x-%x-%x-%x",cmd->temp_buffer[1],cmd->temp_buffer[2],cmd->temp_buffer[3],cmd->temp_buffer[4],cmd->temp_buffer[5],cmd->temp_buffer[6]);
						//ESP_LOGI(TAG,"0x54 :%s",node->pack_info->pack_about.type_ascii);
						ESP_LOGI(TAG,"0x54 :%s",Inv_can[0].pack_data[0].pack_about.type_ascii);
					}
					break;
					case UDT_QUERY_CMD:
					{
						ESP_LOGW(TAG, "udt pc interface, received data length: %d, slave_addr: %d, funcode: %d",
						data_len, cmd->temp_buffer[10], cmd->temp_buffer[11]);
						// ESP_LOG_BUFFER_HEX(TAG, cmd->temp_buffer, cmd->write_total_len);
						udt_pc_interface(cmd->temp_buffer, data_len);
					}
					break;
					case 0x41:
					{
						ESP_LOGW(TAG, "srcaddr:0x%x, 0x41 temp_buffer:0x%x-0x%x-0x%x-0x%x",RxcanId.bit.src, cmd->temp_buffer[25], cmd->temp_buffer[26], cmd->temp_buffer[27], cmd->temp_buffer[28]);
						ESP_LOG_BUFFER_HEX(TAG, &cmd->temp_buffer[20], 48);
						for(uint8_t m=0;m<3;m++)
						{
							ESP_LOGW(TAG,"INV_CAN 0X41 Get:");
							//ESP_LOG_BUFFER_HEX(TAG, (char *)(&Inv_can[m].dc_hub_data[0].dc_hub_info),68);
						}
					}
					break;
				}

				#if 0
				if(type==0x14)
				{
					ESP_LOGI(TAG,"0X14 rev write_offset:%u,data_len:%u,%x-%x-%x-%x",cmd->write_offset,data_len,cmd->temp_buffer[18],cmd->temp_buffer[19],cmd->temp_buffer[20],cmd->temp_buffer[21]);
					ESP_LOGI(TAG, "0X14 Inv_can[0].inv_pv[0].input_voltage =%u,inv_pv.input_voltage =%u",Inv_can[0].inv_data[0].inv_pv.pv_detail[0].input_power,Inv_can[0].inv_data[0].inv_pv.pv_detail[0].input_voltage);
					ESP_LOGI(TAG, "0X14 Inv_can[0].inv_pv[1].input_voltage =%u,inv_pv.input_voltage =%u",Inv_can[0].inv_data[0].inv_pv.pv_detail[1].input_power,Inv_can[0].inv_data[0].inv_pv.pv_detail[1].input_voltage);
				}

				if(type == 0x54)
				{
					ESP_LOGI(TAG,"0X54 rev %x-%x-%x-%x-%x-%x",cmd->temp_buffer[1],cmd->temp_buffer[2],cmd->temp_buffer[3],cmd->temp_buffer[4],cmd->temp_buffer[5],cmd->temp_buffer[6]);
					//ESP_LOGI(TAG,"0x54 :%s",node->pack_info->pack_about.type_ascii);
					ESP_LOGI(TAG,"0x54 :%s",Inv_can[0].pack_data[0].pack_about.type_ascii);
				}else if (type == UDT_QUERY_CMD)
				{
					ESP_LOGW(TAG, "udt pc interface, received data length: %d, slave_addr: %d, funcode: %d",
							data_len, cmd->temp_buffer[10], cmd->temp_buffer[11]);
					// ESP_LOG_BUFFER_HEX(TAG, cmd->temp_buffer, cmd->write_total_len);
					udt_pc_interface(cmd->temp_buffer, data_len);
				}
				else if(type == 0x11)
				{
					ESP_LOGW(TAG, "0x11, buf:0x%x-0x%x-0x%x",
						 cmd->temp_buffer[7], cmd->temp_buffer[8], cmd->temp_buffer[9]);
					//ESP_LOGI(TAG,"remote_sleep_on:%d",Inv_can[1].inv_data[0].inv_base.ctrl_status.bit.remote_sleep_on);
					ESP_LOGI(TAG,"0x11,inv_base.PvTotalChargingEnergy:%lu",Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_base.PvTotalChargingEnergy);
				}
				else if (type == UDT_RESP_CMD)
				{
					ESP_LOGW(TAG, "udt received device data and relay it to %s terminal", (udt_mode_get()==UDT_MODE_PC)?("pc"):("svc"));
					udt_protocol_t *udt_prot = (udt_protocol_t *)cmd->temp_buffer;
					ESP_LOGW(TAG, "dev_id: %02x, data_len: %d, slave_addr: %d, funcode: %d, pack_idx: %d, pack_total: %d\n",
							devId, data_len, udt_prot->address, udt_prot->funcode, UDT_SWAP16(udt_prot->pkg_idx), UDT_SWAP16(udt_prot->pkg_total));
					// ESP_LOG_BUFFER_HEX(TAG, cmd->temp_buffer, 20);
					udt_relay_to_terminal(cmd->temp_buffer, data_len);
				}
				else if (type == INV_TYPE_ABOUT_13H)
				{
					CAN_DEV_SN_Get(devId);
				}
				else if (type == 0x42)
				{
					ESP_LOGW(TAG, "srcaddr:0x%x, 0x42 temp_buffer:0x%x-0x%x-0x%x-0x%x",RxcanId.bit.src, cmd->temp_buffer[21], cmd->temp_buffer[22], cmd->temp_buffer[23], cmd->temp_buffer[24]);
					ESP_LOG_BUFFER_HEX(TAG, &cmd->temp_buffer[0], 40);
					ESP_LOGW(TAG,"INV_CAN 0X42 Get:%d",(unsigned int)Inv_can_mix.ac_hub_data[0].ac_hub_info.SoftwareVersion);
					//ESP_LOG_BUFFER_HEX(TAG, (char *)(&Inv_can_mix.ac_hub_data[0].ac_hub_info),68);
				}
				else if (type == 0x41)
				{
					ESP_LOGW(TAG, "srcaddr:0x%x, 0x41 temp_buffer:0x%x-0x%x-0x%x-0x%x",RxcanId.bit.src, cmd->temp_buffer[25], cmd->temp_buffer[26], cmd->temp_buffer[27], cmd->temp_buffer[28]);
					 ESP_LOG_BUFFER_HEX(TAG, &cmd->temp_buffer[20], 48);
					for(uint8_t m=0;m<3;m++)
					{
						ESP_LOGW(TAG,"INV_CAN 0X41 Get:");
						//ESP_LOG_BUFFER_HEX(TAG, (char *)(&Inv_can[m].dc_hub_data[0].dc_hub_info),68);
					}
				}
				else if (type == IOT_TYPE_SET_02H)
				{
					if(WRITE_DATA == RxcanId.bit.funcode)
					{
						reals.ModbusCmdFlag.sBit.iot_can_set_mix =1;
						Can_iot_callback_set_type_0x2(cmd->write_offset, data_len);
					}
				}
				else if (type == INV_TYPE_CONFIG00_1AH)
				{
					if(WRITE_DATA == RxcanId.bit.funcode)
					{
						ESP_LOGE(TAG,"INV_TYPE_CONFIG00_1AH cmd->write_offset: %d, data_len: %d", cmd->write_offset, data_len);
						if(cmd->write_offset == 0x0f)
						{
							// 各个单逆变的
							memcpy((uint8_t *)&(*node->invs_info)[0].inv_set00.ctrl_ac,
								(uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl_ac, sizeof(Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl_ac));//AC开关
							// 逆变汇总的
							memcpy((uint8_t *)&(*node->invs_info)[INV_MAX_NUM].inv_set00.ctrl_ac,
								(uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl_ac, sizeof(Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl_ac));//AC开关
							ESP_LOGI(TAG,"000---Inv_can[0].ctrl_ac:%d,Inv_can[1].ctrl_ac:%d,Inv_can[2].ctrl_ac:%d,Inv_can[3].ctrl_ac:%d",Inv_can[0].inv_data[0].inv_set00.ctrl_ac,Inv_can[1].inv_data[0].inv_set00.ctrl_ac,Inv_can[2].inv_data[0].inv_set00.ctrl_ac,Inv_can[3].inv_data[0].inv_set00.ctrl_ac);
							ESP_LOGI(TAG,"111---Inv_can[0].ctrl_ac:%d,Inv_can[1].ctrl_ac:%d,Inv_can[2].ctrl_ac:%d,Inv_can[3].ctrl_ac:%d",Inv_can[0].inv_data[1].inv_set00.ctrl_ac,Inv_can[1].inv_data[1].inv_set00.ctrl_ac,Inv_can[2].inv_data[1].inv_set00.ctrl_ac,Inv_can[3].inv_data[1].inv_set00.ctrl_ac);
						}
						else if(cmd->write_offset == 0x10)
						{
							// 各个单逆变的
							memcpy((uint8_t *)&(*node->invs_info)[0].inv_set00.ctrl_dc,
								(uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl_dc, sizeof(Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl_dc));//DC开关
							// 逆变汇总的
							memcpy((uint8_t *)&(*node->invs_info)[INV_MAX_NUM].inv_set00.ctrl_dc,
								(uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl_dc, sizeof(Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl_dc));//DC开关
							//ESP_LOGI(TAG,"INV_TYPE_CONFIG00_10H ctrl_dc:%x  0x%x",Inv_can[0].inv_data[0].inv_set00.self_config.all,Inv_can_WR.bk_inv_dev_set.inv_set00.self_config.all);
							ESP_LOGI(TAG,"CanVerifyData Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.ctrl_dc:%d ",Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.ctrl_dc);
						}
						else if(cmd->write_offset == 0x5e)
						{
							// 各个单逆变的
							memcpy((uint8_t *)&(*node->invs_info)[0].inv_set00.self_config,
								(uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00.self_config, sizeof(Inv_can_WR.bk_inv_dev_set.inv_set00.self_config));//自定义开关
							// 逆变汇总的
							memcpy((uint8_t *)&(*node->invs_info)[INV_MAX_NUM].inv_set00.self_config,
								(uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00.self_config, sizeof(Inv_can_WR.bk_inv_dev_set.inv_set00.self_config));//自定义开关
							ESP_LOGI(TAG,"INV_TYPE_CONFIG00_1AH self_config:%x  0x%x",Inv_can[0].inv_data[0].inv_set00.self_config.all,Inv_can_WR.bk_inv_dev_set.inv_set00.self_config.all);
						}
						else if(cmd->write_offset == 0x1a)
						{
							// 各个单逆变的
							(*node->invs_info)[0].inv_set00.ctrl_chg_mode= Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl_chg_mode;
							// 逆变汇总的
							(*node->invs_info)[INV_MAX_NUM].inv_set00.ctrl_chg_mode= Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl_chg_mode;
							ESP_LOGI(TAG,"INV_TYPE_CONFIG00_1AH ctrl_chg_mode:0x%x  0x%x",Inv_can[0].inv_data[0].inv_set00.ctrl_chg_mode,Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl_chg_mode);
						}
						else if(cmd->write_offset == 0x60)
						{
							// 各个单逆变的
							memcpy((uint8_t *)&(*node->invs_info)[0].inv_set00.remoteSet,
								(uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00.remoteSet,sizeof(Inv_can_WR.bk_inv_dev_set.inv_set00.remoteSet));
							// 逆变汇总的
							memcpy((uint8_t *)&(*node->invs_info)[INV_MAX_NUM].inv_set00.remoteSet,
								(uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00.remoteSet,sizeof(Inv_can_WR.bk_inv_dev_set.inv_set00.remoteSet));
						}
						else if(cmd->write_offset == 0x66)
						{
							// 各个单逆变的
							memcpy((uint8_t *)&(*node->invs_info)[0].inv_set00.LevelSwitch,
								(uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00.LevelSwitch,sizeof(Inv_can_WR.bk_inv_dev_set.inv_set00.LevelSwitch));
							// 逆变汇总的
							memcpy((uint8_t *)&(*node->invs_info)[INV_MAX_NUM].inv_set00.LevelSwitch,
								(uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00.LevelSwitch,sizeof(Inv_can_WR.bk_inv_dev_set.inv_set00.LevelSwitch));
							//(*node->invs_info)[INV_MAX_NUM].inv_set00.LevelSwitch= Inv_can_WR.bk_inv_dev_set.inv_set00.LevelSwitch;
							ESP_LOGI(TAG,"INV_TYPE_CONFIG00_1AH LevelSwitch.bit.level:%d  %d",Inv_can[0].inv_data[0].inv_set00.LevelSwitch.bit.level,Inv_can_WR.bk_inv_dev_set.inv_set00.LevelSwitch.bit.level);
						}

					}
					//ESP_LOGI(TAG,"INV_TYPE_CONFIG00_1AH self_config.all:0x%x  0x%x",Inv_can[0].inv_data[0].inv_set00.self_config.all,Inv_can_WR.bk_inv_dev_set.inv_set00.self_config.all);
					//ESP_LOGI(TAG,"INV_TYPE_CONFIG00_1AH LevelSwitch.bit.level:%d  %d",Inv_can[0].inv_data[0].inv_set00.LevelSwitch.bit.level,Inv_can_WR.bk_inv_dev_set.inv_set00.LevelSwitch.bit.level);
					//ESP_LOGI(TAG,"INV_TYPE_CONFIG00_1AH3 LevelSwitch.bit.level:%d  %d",Inv_can[3].inv_data[0].inv_set00.LevelSwitch.bit.level,Inv_can_WR.bk_inv_dev_set.inv_set00.LevelSwitch.bit.level);
				}
				else if (type == INV_TYPE_CONFIG01_1BH)
				{
					if(WRITE_DATA == RxcanId.bit.funcode)
					{
						ESP_LOGE(TAG,"INV_TYPE_CONFIG00_1AH cmd->write_offset: %d, data_len: %d", cmd->write_offset, data_len);
						if(cmd->write_offset == 0x70)
						{
							// 各个单逆变的
							memcpy((uint8_t *)&(*node->invs_info)[0].inv_set01.Func_Set,
								(uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01.Func_Set, sizeof(Inv_can_WR.bk_inv_dev_set.inv_set01.Func_Set));//AC开关
							// 逆变汇总的
							memcpy((uint8_t *)&(*node->invs_info)[INV_MAX_NUM].inv_set01.Func_Set,
								(uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01.Func_Set, sizeof(Inv_can_WR.bk_inv_dev_set.inv_set01.Func_Set));//AC开关
							ESP_LOGI(TAG,"000---Inv_can[0].Func_Set:%u,Inv_can[1].ctrl_ac:%u,Inv_can[2].ctrl_ac:%u,Inv_can[3].ctrl_ac:%u",Inv_can[0].inv_data[0].inv_set01.Func_Set.all,Inv_can[1].inv_data[0].inv_set01.Func_Set.all,Inv_can[2].inv_data[0].inv_set01.Func_Set.all,Inv_can[3].inv_data[0].inv_set01.Func_Set.all);
							ESP_LOGI(TAG,"111---Inv_can[0].Func_Set:%u,Inv_can[1].ctrl_ac:%u,Inv_can[2].ctrl_ac:%u,Inv_can[3].ctrl_ac:%u",Inv_can[0].inv_data[1].inv_set01.Func_Set.all,Inv_can[1].inv_data[1].inv_set01.Func_Set.all,Inv_can[2].inv_data[1].inv_set01.Func_Set.all,Inv_can[3].inv_data[1].inv_set01.Func_Set.all);
						}
						else if(cmd->write_offset == 0x6E)
						{
							// 各个单逆变的
							memcpy((uint8_t *)&(*node->invs_info)[0].inv_set01.SetGridMaxCurrent_in,
								(uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01.SetGridMaxCurrent_in, sizeof(Inv_can_WR.bk_inv_dev_set.inv_set01.Func_Set));//AC开关
							// 逆变汇总的
							memcpy((uint8_t *)&(*node->invs_info)[INV_MAX_NUM].inv_set01.SetGridMaxCurrent_in,
								(uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01.SetGridMaxCurrent_in, sizeof(Inv_can_WR.bk_inv_dev_set.inv_set01.Func_Set));//AC开关
							ESP_LOGI(TAG,"000---Inv_can[0].SetGridMaxCurrent_in:%u,Inv_can[1].ctrl_ac:%u,Inv_can[2].ctrl_ac:%u,Inv_can[3].ctrl_ac:%u",Inv_can[0].inv_data[0].inv_set01.Func_Set.all,Inv_can[1].inv_data[0].inv_set01.SetGridMaxCurrent_in,Inv_can[2].inv_data[0].inv_set01.SetGridMaxCurrent_in,Inv_can[3].inv_data[0].inv_set01.SetGridMaxCurrent_in);
							ESP_LOGI(TAG,"111---Inv_can[0].SetGridMaxCurrent_in:%u,Inv_can[1].ctrl_ac:%u,Inv_can[2].ctrl_ac:%u,Inv_can[3].ctrl_ac:%u",Inv_can[0].inv_data[1].inv_set01.Func_Set.all,Inv_can[1].inv_data[1].inv_set01.SetGridMaxCurrent_in,Inv_can[2].inv_data[1].inv_set01.SetGridMaxCurrent_in,Inv_can[3].inv_data[1].inv_set01.SetGridMaxCurrent_in);
						}
					}
				}
				else if (type == MODULE_TYPE_DC_AC_HUB_SET_40H)
				{
					ESP_LOGE(TAG,"MODULE_TYPE_DC_AC_HUB_SET_40H cmd->write_offset: %d, data_len: %d", cmd->write_offset, data_len);
					if(WRITE_DATA == RxcanId.bit.funcode)
					{
						if(cmd->write_offset == 0x02)
						{
							// ACHUB只有总的
							(*node->achubs_info)[0].dc_ac_hub_setting.ac_hug_setting.bit.ac_switch = Inv_can_mix_WR.dc_ac_hub_setting.ac_hug_setting.bit.ac_switch;//HA1开关
						}
					}

				}
				#endif

            }
			else
			{
                #ifdef STATE_COUNT
                cmd->error_cnt++;
                #endif
                ESP_LOGE(TAG, "devId: 0x%02x type: 0x%02x offset: %d len: %d,maxlen=%ld, error", devId, type, cmd->write_offset, data_len,maxlen);
            }
        }
		else
		{
            cmd->crc_valid = 0; /* crc错误 */
        }

        free(cmd->temp_buffer);
        cmd->temp_buffer = NULL;

        if (cmd->crc_valid == 0)
		{
            ESP_LOGE(TAG, "devId: 0x%02x type: 0x%02x len: %d crcA: 0x%04x, calc: 0x%04x", devId, type, data_len, cmd->write_crc16,crc16);
            return ERR_CRC; /* crc错误 */
        }

		if(reals.Step_can_dev_parallel >= STEP_CAN_PARALLEL_AFTER)//
		{

			if((uint32_t)ptr == (uint32_t)&Inv_can[reals.Addr_can_self].inv_data[0].inv_set00)//地址相同
			{
				Inv_can[reals.Addr_can_self].inv_data[0].setdata_valid |=1<<CAN_INV_SETDATA_set00;
			}
			else if((uint32_t)ptr == (uint32_t)&Inv_can[reals.Addr_can_self].inv_data[0].inv_set01)//地址相同
			{
				Inv_can[reals.Addr_can_self].inv_data[0].setdata_valid |=1<<CAN_INV_SETDATA_set01;
			}
			else if((uint32_t)ptr == (uint32_t)&Inv_can[reals.Addr_can_self].inv_data[0].inv_set03)//地址相同
			{
				Inv_can[reals.Addr_can_self].inv_data[0].setdata_valid |=1<<CAN_INV_SETDATA_set03;
			}
			else if((uint32_t)ptr == (uint32_t)&Inv_can[reals.Addr_can_self].inv_data[0].auth_param)//地址相同
			{
				Inv_can[reals.Addr_can_self].inv_data[0].setdata_valid |=1<<CAN_INV_SETDATA_auth_param;
			}
		}

        CAN_To_Modbus_Read_Info_Process(type);//
		//CAN_Dev_Ctrl_SetData_Check(type);
        if (type == 0xFF) {
//            debug_detail_value(node, devId, type);
        }
    }

    return ERR_NONE;
}

//void debug_detail_value(node_info_t *node, uint8_t devid, uint8_t type)
//{
//	ESP_LOGI (TAG, "iot safet: %llu", g_device_data.iot_dev_node.factory.safetCode);
//	ESP_LOGI (TAG, "iot type: %s",    g_device_data.iot_dev_node.factory.type);
//	ESP_LOGI (TAG, "iot sn: %llu",    g_device_data.iot_dev_node.factory.sn);
//}

//#ifdef STATE_COUNT
#if 0
void state_count_statistics(void)
{
    uint8_t *ptr;
    uint32_t len;
    rw_cmd_struct *cmd;

    uint8_t max = sizeof(g_abstract_array_inv)/sizeof(g_abstract_array_inv[0]);
    printf("DevId\tDatType\t\tInterval\t\tSend\t\tRecv\t\tError\r\n");
    for (uint8_t i = 0; i < max; i++)
    {
        CanLookupTypePosition(g_cmd_array_inv[i].abstract->addr,
                              g_cmd_array_inv[i].abstract->type,
                                &ptr, &len, &cmd);
        printf("0x%x\t0x%x\t\t%8d\t\t%u\t\t%u\t\t%u\r\n",
                g_cmd_array_inv[i].abstract->addr,
                g_cmd_array_inv[i].abstract->type,
                g_cmd_array_inv[i].abstract->internal,
                cmd->send_cnt,
                cmd->recv_cnt,
                cmd->error_cnt);
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    max = sizeof(g_abstract_array_pack)/sizeof(g_abstract_array_pack[0]);
    printf("DevId\tDatType\t\tInterval\t\tSend\t\tRecv\t\tError\r\n");
    for (uint8_t j = 0; j < g_device_data.pack_node[0].pack_announce.pack_cnt; j++)
    {
        for (uint8_t i = 0; i < max; i++)
        {
            CanLookupTypePosition(g_cmd_array_pack[i].abstract->addr+j,
                                    g_cmd_array_pack[i].abstract->type,
                                    &ptr, &len, &cmd);
            printf("0x%x\t0x%x\t\t%8d\t\t%u\t\t%u\t\t%u\r\n", \
                    g_cmd_array_pack[i].abstract->addr+j,
                    g_cmd_array_pack[i].abstract->type,
                    g_cmd_array_pack[i].abstract->internal,
                    cmd->send_cnt,
                    cmd->recv_cnt,
                    cmd->error_cnt);
        }
    }
}
#endif

/* 此函数用于把从ARM读取的CAN数据赋值给将要发给app、蓝牙上位机的modbus表，顺序以CAN结构体向下排列，部分需要快速响应或者特殊处理的放在对应的data_summary.c
Inv_can ->Inv
can->modbus

*/
void CAN_To_Modbus_Read_Info_Process(uint8_t type)
{
	    //Inv_can[num].inv_data->inv_base
	    // 0x11 -> 	120-
	int numpack = 0;
	int num = 0;

///////////////PACK汇总的汇总
		num = DEV_MAIN_NODE_MAX;
		numpack = PACK_MAX_NUM;

		// 寄存器1需要的机器标准 需要app一连上就立马有值 需要在Iot_Self_Data_To_Can_data中再次赋值，使其不被g_self_data.mod_reg00000覆盖
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00000.match_stander.voltage_level, (uint8_t *)&Inv_can[reals.online_X_inv_index].inv_data[0].inv_about.voltage_lable,sizeof(Inv_can[0].inv_data[0].inv_about.voltage_lable));// 1 低字节
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00000.match_stander.area, (uint8_t *)&Inv_can[reals.online_X_inv_index].inv_data[0].inv_set01.ctrl_user_area,sizeof(Inv_can[0].inv_data[0].inv_set01.ctrl_user_area));// 1 高字节
		// SetData.dev_info_t.match_stander.voltage_level = Inv_can[reals.online_X_inv_index].inv_data[0].inv_about.voltage_lable;// 先传给SetData，再传给g_self_data，最后赋给Inv
		// SetData.dev_info_t.match_stander.area = Inv_can[reals.online_X_inv_index].inv_data[0].inv_set01.ctrl_user_area;// 先传给SetData，再传给g_self_data，最后赋给Inv

		// ESP_LOGW(TAG, "Inv[DEV_MAIN_NODE_MAX].mod_reg00000.match_stander.voltage_level = %d", Inv[DEV_MAIN_NODE_MAX].mod_reg00000.match_stander.voltage_level);
		// ESP_LOGW(TAG, "Inv[DEV_MAIN_NODE_MAX].mod_reg00000.match_stander.area = %d", Inv[DEV_MAIN_NODE_MAX].mod_reg00000.match_stander.area);
		// Inv[DEV_MAIN_NODE_MAX].mod_reg00000.match_stander.area = 1;// 1 高字节 test ARM未标定，此值暂取假值

		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,total_voltage), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,total_voltage),(offsetof(pack_announce_struct,soc) - offsetof(pack_announce_struct,total_voltage)));// 100~101
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,soc), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,soc),sizeof(Inv_can[num].pack_data[numpack].pack_announce.soc));// 102
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,chg_status), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,chg_status),sizeof(Inv_can[num].pack_data[numpack].pack_announce.chg_status));// 103
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,chg_full_time), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,chg_full_time),(offsetof(pack_announce_struct,is_high_volt) - offsetof(pack_announce_struct,chg_full_time)));// 104~105
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,aging_status), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,aging_status),sizeof(Inv_can[num].pack_data[numpack].pack_announce.aging_status));// 106 //modbus:16,CAN:8
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,PackCnts), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,pack_cnt),sizeof(Inv_can[num].pack_data[numpack].pack_announce.pack_cnt));// 107
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,PackOnline), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,online),sizeof(Inv_can[num].pack_data[numpack].pack_announce.online));// 108

		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,total_voltage), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,total_voltage),(offsetof(pack_announce_struct,soc) - offsetof(pack_announce_struct,total_voltage)));// 6003~6004
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,soc), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,soc),sizeof(Inv_can[num].pack_data[numpack].pack_announce.soc));// 6005
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,soh), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,soh),sizeof(Inv_can[num].pack_data[numpack].pack_announce.soh));// 6006
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,avg_temp), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,avg_temp),sizeof(Inv_can[num].pack_data[numpack].pack_announce.avg_temp));// 6007
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,work_status), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,work_status),sizeof(Inv_can[num].pack_data[numpack].pack_announce.work_status));// 6008
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,chg_status), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,chg_status),sizeof(Inv_can[num].pack_data[numpack].pack_announce.chg_status));// 6009
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,max_chg_voltage), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,max_chg_voltage),(offsetof(pack_announce_struct,status1) - offsetof(pack_announce_struct,max_chg_voltage)));// 6010~6012
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,status1), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,status1),(offsetof(pack_announce_struct,soh) - offsetof(pack_announce_struct,status1)));// 6013~6014
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,cap_online), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,cap_online),sizeof(Inv_can[num].pack_data[numpack].pack_announce.cap_online));// 6016
		// memcpy((uint8_t *)&Inv[num*PACK_MAX_NUM +numpack].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,cap_online), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,pack_cap_online),sizeof(Inv_can[num].pack_data[numpack].pack_announce.pack_cap_online));// 6016
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,chg_full_time), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,chg_full_time),(offsetof(pack_announce_struct,is_high_volt) - offsetof(pack_announce_struct,chg_full_time)));// 6017~6018
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,total_chg_energy), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_extend + offsetof(pack_extend_struct,total_chg_energy),(offsetof(pack_extend_struct,once_chg_energy) - offsetof(pack_extend_struct,total_chg_energy)));// 6019~6022
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,TotalCurrent_bias), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,TotalCurrent_bias),sizeof(Inv_can[num].pack_data[numpack].pack_announce.TotalCurrent_bias));//

//////////////单组逆变器的PACK汇总
		for( num = 0;num < (DEV_MAIN_NODE_MAX);num++)//
		{
			// if(type == 0x50) //pack广播消息，主包汇总
			numpack =0;//单逆变器系统的主PACK 序号=0， pack_announce

			memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,total_voltage), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,total_voltage),(offsetof(pack_announce_struct,soc) - offsetof(pack_announce_struct,total_voltage)));// 100~101
			memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,soc), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,soc),sizeof(Inv_can[num].pack_data[numpack].pack_announce.soc));// 102
			memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,chg_status), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,chg_status),sizeof(Inv_can[num].pack_data[numpack].pack_announce.chg_status));// 103
			memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,chg_full_time), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,chg_full_time),(offsetof(pack_announce_struct,is_high_volt) - offsetof(pack_announce_struct,chg_full_time)));// 104~105
			memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,aging_status), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,aging_status),sizeof(Inv_can[num].pack_data[numpack].pack_announce.aging_status));// 106 //modbus:16,CAN:8
			memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,PackCnts), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,pack_cnt),sizeof(Inv_can[num].pack_data[numpack].pack_announce.pack_cnt));// 107
			memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,PackOnline), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,online),sizeof(Inv_can[num].pack_data[numpack].pack_announce.online));// 108
			if (reals.IOT_Status_Flag.sBit.system_sleep_flag) {
				Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1.Iot_State.bit.remote_off_state = 1;
				Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1.Parts_online.bit.remote_off_en = 1;	// 休眠期间保持支持远程关机标志
			} else {
				Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1.Iot_State.bit.remote_off_state =
					Inv_can[num].inv_data[numpack].inv_base.ctrl_status.bit.remote_sleep_on;
				Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1.Parts_online.bit.remote_off_en=Inv_can[num].inv_data[numpack].inv_set00.remoteSet.bit.remoteOffCtrlEn;
			}


			memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,total_voltage), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,total_voltage),(offsetof(pack_announce_struct,soc) - offsetof(pack_announce_struct,total_voltage)));// 6003~6004
			memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,soc), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,soc),sizeof(Inv_can[num].pack_data[numpack].pack_announce.soc));// 6005
			memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,soh), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,soh),sizeof(Inv_can[num].pack_data[numpack].pack_announce.soh));// 6006
			memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,avg_temp), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,avg_temp),sizeof(Inv_can[num].pack_data[numpack].pack_announce.avg_temp));// 6007
			memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,work_status), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,work_status),sizeof(Inv_can[num].pack_data[numpack].pack_announce.work_status));// 6008
			memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,chg_status), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,chg_status),sizeof(Inv_can[num].pack_data[numpack].pack_announce.chg_status));// 6009
			memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,max_chg_voltage), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,max_chg_voltage),(offsetof(pack_announce_struct,status1) - offsetof(pack_announce_struct,max_chg_voltage)));// 6010~6012
			memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,status1), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,status1),(offsetof(pack_announce_struct,soh) - offsetof(pack_announce_struct,status1)));// 6013~6014
			memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,cap_online), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,cap_online),sizeof(Inv_can[num].pack_data[numpack].pack_announce.cap_online));// 6016
			// memcpy((uint8_t *)&Inv[num*PACK_MAX_NUM +numpack].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,cap_online), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,pack_cap_online),sizeof(Inv_can[num].pack_data[numpack].pack_announce.pack_cap_online));// 6016
			memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,chg_full_time), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,chg_full_time),(offsetof(pack_announce_struct,is_high_volt) - offsetof(pack_announce_struct,chg_full_time)));// 6017~6018
			memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,total_chg_energy), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_extend + offsetof(pack_extend_struct,total_chg_energy),(offsetof(pack_extend_struct,once_chg_energy) - offsetof(pack_extend_struct,total_chg_energy)));// 6019~6022
			memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,TotalCurrent_bias), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_announce + offsetof(pack_announce_struct,TotalCurrent_bias),sizeof(Inv_can[num].pack_data[numpack].pack_announce.TotalCurrent_bias));//
		}

//////////////INV 汇总	CAN的汇总到modbus的汇总 此处待放入data_summary.c
		num = DEV_MAIN_NODE_MAX;
		numpack = INV_MAX_NUM;


		//test xf
		/* modbus 1100 */
		// memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvType), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_about + offsetof(inv_about_struct,dev_type),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_about.dev_type));// 1101~1106  ASCII 机型(单个)
		// memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvSN), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_about + offsetof(inv_about_struct,dev_sn),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_about.dev_sn));// 1107~1110  设备唯一识别SN码(单个)

		// memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,DevVoltageLable), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_about + offsetof(inv_about_struct,voltage_lable),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_about.voltage_lable));// 1149
		// memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,WorkTimeNumber), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_about + offsetof(inv_about_struct,time_area_num),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_about.time_area_num));// 1148
		// memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,software_total), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_about + offsetof(inv_about_struct,software_total),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_about.software_total));// 1112
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvChgLimitL1), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,inv_chg_limit_L1),sizeof(Inv_can[num].inv_data[numpack].inv_about.inv_chg_limit_L1));// 1155
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvChgLimitL2), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,inv_chg_limit_L2),sizeof(Inv_can[num].inv_data[numpack].inv_about.inv_chg_limit_L2));// 1156
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvChgLimitL3), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,inv_chg_limit_L3),sizeof(Inv_can[num].inv_data[numpack].inv_about.inv_chg_limit_L3));// 1157
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvDisgLimitL1), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,inv_disg_limit_L1),sizeof(Inv_can[num].inv_data[numpack].inv_about.inv_disg_limit_L1));// 1158
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvDisgLimitL2), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,inv_disg_limit_L2),sizeof(Inv_can[num].inv_data[numpack].inv_about.inv_disg_limit_L2));// 1159
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvDisgLimitL3), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,inv_disg_limit_L3),sizeof(Inv_can[num].inv_data[numpack].inv_about.inv_disg_limit_L3));// 1160
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,machineL1AcInputRatedCurrent), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,ac_input_rated_current_L1),sizeof(Inv_can[num].inv_data[numpack].inv_about.ac_input_rated_current_L1));// 1161
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,machineL2AcInputRatedCurrent), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,ac_input_rated_current_L2),sizeof(Inv_can[num].inv_data[numpack].inv_about.ac_input_rated_current_L2));// 1162
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,machineL3AcInputRatedCurrent), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,ac_input_rated_current_L3),sizeof(Inv_can[num].inv_data[numpack].inv_about.ac_input_rated_current_L3));// 1163
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,machineL1AcOutputRatedCurrent), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,ac_input_rated_current_L1),sizeof(Inv_can[num].inv_data[numpack].inv_about.ac_output_rated_current_L1));// 1164
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,machineL2AcOutputRatedCurrent), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,ac_input_rated_current_L2),sizeof(Inv_can[num].inv_data[numpack].inv_about.ac_output_rated_current_L2));// 1165
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,machineL3AcOutputRatedCurrent), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,ac_input_rated_current_L3),sizeof(Inv_can[num].inv_data[numpack].inv_about.ac_output_rated_current_L3));// 1166
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,gridL1AcOutputRatedCurrent), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,grid_ac_output_rated_current_L1),sizeof(Inv_can[num].inv_data[numpack].inv_about.grid_ac_output_rated_current_L1));// 1167
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,gridL2AcOutputRatedCurrent), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,grid_ac_output_rated_current_L2),sizeof(Inv_can[num].inv_data[numpack].inv_about.grid_ac_output_rated_current_L2));// 1168
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,gridL3AcOutputRatedCurrent), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,grid_ac_output_rated_current_L3),sizeof(Inv_can[num].inv_data[numpack].inv_about.grid_ac_output_rated_current_L3));// 1169

		/* modbus 1200 */
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01200_Inv_pv + offsetof(MOD_STRUCT_reg01200,total_chg_power), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_pv + offsetof(inv_pv_struct,total_chg_power),(offsetof(inv_pv_struct,pv_number) - offsetof(inv_pv_struct,total_chg_power)));// 1200~1203
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01200_Inv_pv + offsetof(MOD_STRUCT_reg01200,pv_number), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_pv + offsetof(inv_pv_struct,pv_number),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_pv.pv_number));// 1209
		//memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01200_Inv_pv + offsetof(MOD_STRUCT_reg01200,pv_detail), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_pv + offsetof(inv_pv_struct,pv_detail),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_pv.pv_detail));// 1210~1289
		for(int i = 0;i < 10;i++)// 1210~1289
		{
			memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01200_Inv_pv.pv_detail[i].status, (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_pv.pv_detail[i].status,sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_pv.pv_detail[i].status));
			memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01200_Inv_pv.pv_detail[i].input_type, (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_pv.pv_detail[i].input_type,sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_pv.pv_detail[i].input_type));
			memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01200_Inv_pv.pv_detail[i].input_power, (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_pv.pv_detail[i].input_power,sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_pv.pv_detail[i].input_power));
			memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01200_Inv_pv.pv_detail[i].input_voltage, (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_pv.pv_detail[i].input_voltage,sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_pv.pv_detail[i].input_voltage));
			memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01200_Inv_pv.pv_detail[i].input_current, (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_pv.pv_detail[i].input_current,sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_pv.pv_detail[i].input_current));
		}


		/* modbus 1300 */
		//memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid + offsetof(MOD_STRUCT_reg01300,freq),
			//(uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_grid + offsetof(inv_grid_struct,freq),(offsetof(inv_grid_struct,grid_phase_number) - offsetof(inv_grid_struct,freq)));// 1300~1306
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid + offsetof(MOD_STRUCT_reg01300,total_chg_energy),
			(uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_grid + offsetof(inv_grid_struct,total_chg_energy),(offsetof(inv_grid_struct,grid_phase_number) - offsetof(inv_grid_struct,total_chg_energy)));// 1303~1306
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid + offsetof(MOD_STRUCT_reg01300,grid_phase_number),
			(uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_grid + offsetof(inv_grid_struct,grid_phase_number),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_grid.grid_phase_number));// 1312
		for(int i = 0;i < 3;i++)// 1313~1330
		{
			//memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.grid_detail[i].input_power,
				//(uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_grid.grid_detail[i].input_power,sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_grid.grid_detail[i].input_power));
			//memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.grid_detail[i].input_voltage,
				//(uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_grid.grid_detail[i].input_voltage,sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_grid.grid_detail[i].input_voltage));
			memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.grid_detail[i].input_current,
				(uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_grid.grid_detail[i].input_current,sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_grid.grid_detail[i].input_current));
		}
		//ESP_LOGI(TAG,"bbb mod_reg01300_Inv_grid.freq:%u,total_chg_power:%lu",Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.freq,Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.total_chg_power);

		/* modbus 1400 */
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load + offsetof(MOD_STRUCT_reg01400,total_dc_load_power), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_load + offsetof(inv_load_struct,total_dc_load_power),(offsetof(inv_load_struct,total_ac_load_power) - offsetof(inv_load_struct,total_dc_load_power)));// 1400~1409
		//memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load + offsetof(MOD_STRUCT_reg01400,total_ac_load_power), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_load + offsetof(inv_load_struct,total_ac_load_power),(offsetof(inv_load_struct,ac_phase_number) - offsetof(inv_load_struct,total_ac_load_power)));// 1420~1423
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load + offsetof(MOD_STRUCT_reg01400,ac_phase_number), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_load + offsetof(inv_load_struct,ac_phase_number),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_load.ac_phase_number));// 1429
		// uint16_t phase_num_test = 2;
		// memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load + offsetof(MOD_STRUCT_reg01400,ac_phase_number), &phase_num_test, 2);// 1429
		for(int i = 0;i < 3;i++)// 1430~1447
		{
			//memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load.ac_load[i].load_power, (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_load.ac_load[i].load_power,sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_load.ac_load[i].load_power));
			memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load.ac_load[i].load_voltage, (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_load.ac_load[i].load_voltage,sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_load.ac_load[i].load_voltage));
			memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load.ac_load[i].load_current, (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_load.ac_load[i].load_current,sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_load.ac_load[i].load_current));
		}


		/* modbus 1500 */
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01500_Inv_inv + offsetof(MOD_STRUCT_reg01500,freq), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_data + offsetof(inv_data_struct,freq),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_data.freq));// 1500
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01500_Inv_inv + offsetof(MOD_STRUCT_reg01500,total_energy), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_data + offsetof(inv_data_struct,total_energy),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_data.total_energy));// 1501~1502 逆变的总能量
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01500_Inv_inv + offsetof(MOD_STRUCT_reg01500,phase_number), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_data + offsetof(inv_data_struct,phase_number),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_data.phase_number));// 1508 相位数量 最多3相
		// memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01500_Inv_inv + offsetof(MOD_STRUCT_reg01500,inv_detail), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_data + offsetof(inv_data_struct,inv_detail),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_data.inv_detail));// 1509~1529 每相详细信息 最多3相
		for(int i = 0;i < 3;i++)// 1509~1529
		{
			Inv[DEV_MAIN_NODE_MAX].mod_reg01500_Inv_inv.inv_detail[i].work_status = Inv_can[num].inv_data[numpack].inv_data.inv_detail[i].work_status;// 1509
			Inv[DEV_MAIN_NODE_MAX].mod_reg01500_Inv_inv.inv_detail[i].power = Inv_can[num].inv_data[numpack].inv_data.inv_detail[i].power;// 1510
			Inv[DEV_MAIN_NODE_MAX].mod_reg01500_Inv_inv.inv_detail[i].voltage = Inv_can[num].inv_data[numpack].inv_data.inv_detail[i].voltage;// 1511
			Inv[DEV_MAIN_NODE_MAX].mod_reg01500_Inv_inv.inv_detail[i].current = Inv_can[num].inv_data[numpack].inv_data.inv_detail[i].current;// 1512
		}


		// ESP_LOGW(TAG,"Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_data.inv_detail[0].power = %d",Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_data.inv_detail[0].power);

		/* modbus 6100 */
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,type_ascii), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_about + offsetof(pack_about_struct,type_ascii),(offsetof(pack_about_struct,software_total) - offsetof(pack_about_struct,type_ascii)));// 6101~6110
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,total_voltage), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,total_voltage),(offsetof(pack_base_struct,soc) - offsetof(pack_base_struct,total_voltage)));// 6111~6112
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,soc), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,soc),sizeof(Inv_can[num].pack_data[numpack].pack_base.soc));// 6113
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,soh), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,soh),sizeof(Inv_can[num].pack_data[numpack].pack_base.soh));// 6114
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,avg_temp), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,avg_temp),sizeof(Inv_can[num].pack_data[numpack].pack_base.avg_temp));// 6115
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,min_cell_voltage), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,min_cell_voltage),(offsetof(pack_base_struct,min_cell_index) - offsetof(pack_base_struct,min_cell_voltage)));// 6116~6117
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,min_cell_index), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,min_cell_index),sizeof(Inv_can[num].pack_data[numpack].pack_base.min_cell_index));// 6118
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,max_cell_index), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,max_cell_index),sizeof(Inv_can[num].pack_data[numpack].pack_base.max_cell_index));// 6119
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,min_temp_value), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,min_temp_value),sizeof(Inv_can[num].pack_data[numpack].pack_base.min_temp_value));// 6120
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,max_temp_value), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,max_temp_value),sizeof(Inv_can[num].pack_data[numpack].pack_base.max_temp_value));// 6121
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,min_temp_index), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,min_temp_index),sizeof(Inv_can[num].pack_data[numpack].pack_base.min_temp_index));// 6122
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,max_temp_index), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,max_temp_index),sizeof(Inv_can[num].pack_data[numpack].pack_base.max_temp_index));// 6123
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,work_status), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,work_status),sizeof(Inv_can[num].pack_data[numpack].pack_base.work_status));// 6124
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,chg_status), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,chg_status),sizeof(Inv_can[num].pack_data[numpack].pack_base.chg_status));// 6125
		// memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,pack_online), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,pack_cap_online),sizeof(Inv_can[num].pack_data[numpack].pack_base.pack_cap_online));// 6127
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,relay), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,relay),sizeof(Inv_can[num].pack_data[numpack].pack_base.relay));// 6128
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,pack_cap_online), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,pack_cap_online),sizeof(Inv_can[num].pack_data[numpack].pack_base.pack_cap_online));// 6129
		// 6130 pack_canbus_error
		// /*6131~6143*/ 0x52

		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,protect), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,protect),(offsetof(pack_base_struct,relay) - offsetof(pack_base_struct,protect)));// 6144~6149
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,chg_full_time), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,protect),(offsetof(pack_base_struct,allow_max_chg_voltage) - offsetof(pack_base_struct,chg_full_time)));// 6150~6151

		// 6152~6153 在AC380中被用作每种类型的总的电芯、探头数量
		// memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,pack_total_cell), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,pack_total_cell),sizeof(Inv_can[num].pack_data[numpack].pack_base.pack_total_cell));// 6152
		// memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,pack_total_ntc), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,pack_total_ntc),sizeof(Inv_can[num].pack_data[numpack].pack_base.pack_total_ntc));// 6153
		// 6154 PackBMUCnt 在AC380中被用作每种类型的电池包数量(.eg:B300K有3个电池包 若B300K是通过从机地址01读取时 该值为3)
		// 6157
		// memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,pack_outsum_voltage), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,vbus),sizeof(Inv_can[num].pack_data[numpack].pack_base.vbus));// 6158
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,allow_max_chg_current), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,allow_max_chg_current),(offsetof(pack_base_struct,protect_status) - offsetof(pack_base_struct,allow_max_chg_current)));// 6160~6161

		// modbus 100
		// memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,inv_num), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,inv_num),sizeof(Inv_can[num].inv_data[numpack].inv_base.inv_num));// 120
		// memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,inv_online), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,inv_online),sizeof(Inv_can[num].inv_data[numpack].inv_base.inv_online));// 121
		// 单boot的逆变在线数量、在线位，安卓端需要有值
		Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.inv_num = reals.online_Inv_num;// 120
		Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.inv_online = reals.online_Inv_bit;// 121
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,inv_power_rang), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,inv_power_rang),sizeof(Inv_can[num].inv_data[numpack].inv_base.inv_power_rang));// 122

		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,inv_power_rang), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,inv_power_rang),sizeof(Inv_can[num].inv_data[numpack].inv_base.inv_power_rang));// 1111(单机)

		// memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,line_event), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,line_event),sizeof(Inv_can[num].inv_data[numpack].inv_base.line_event));// 123
		//memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,ctrl_status), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,ctrl_status),sizeof(Inv_can[num].inv_data[numpack].inv_base.ctrl_status));// 124
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,GridandMachineSOC), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,grid_par_soc),sizeof(Inv_can[num].inv_data[numpack].inv_base.grid_par_soc));// 125
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,alarm), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,alarm),sizeof(Inv_can[num].inv_data[numpack].inv_base.alarm));// 126~129
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,fault), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,fault),sizeof(Inv_can[num].inv_data[numpack].inv_base.fault));// 133~136 故障信息
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,fault[4]), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,fault5),sizeof(Inv_can[num].inv_data[numpack].inv_base.fault5));// 137 故障信息
		CAN_SetAlarmStateFromAcHub(DEV_MAIN_NODE_MAX,0);

		// memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,DCLoadAllTotalPower), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,DCLoadAllTotalPower),(offsetof(inv_base_struct,fault5) - offsetof(inv_base_struct,DCLoadAllTotalPower)));// 140~159
		// uint16_t  ctrl_chg_mode;  // 160 控制充电模式 0x1a
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,inv_work_state), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,inv_work_state),sizeof(Inv_can[num].inv_data[numpack].inv_base.inv_work_state));// 161
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,PvToACLoadEnergy), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,PvToACLoadEnergy),sizeof(Inv_can[num].inv_data[numpack].inv_base.PvToACLoadEnergy));// 162~163
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,SelfConsumptionPercent), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,SelfConsumptionPercent),sizeof(Inv_can[num].inv_data[numpack].inv_base.SelfConsumptionPercent));// 164
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,PVToACloadPower), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,PVToACloadPower),sizeof(Inv_can[num].inv_data[numpack].inv_base.PVToACloadPower));// 165~166
		Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.SwitchMemoryState = Inv_can[num].inv_data[numpack].inv_base.switch_memory_state;// 191

		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,Rated_Voltage), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,rated_Voltage),sizeof(Inv_can[num].inv_data[numpack].inv_about.rated_Voltage));// 169~170
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,Rated_Frequency), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,rated_Frequency),sizeof(Inv_can[num].inv_data[numpack].inv_about.rated_Frequency));// 171~172
		// memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,Rated_Voltage), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,ctrl_output_inv_volt),(offsetof(inv_set01_struct,ctrl_chg_max_volt) - offsetof(inv_set01_struct,ctrl_output_inv_volt)));// 169~170
		// 171 配件在线状态由CanListenBus()获取并传递给app
		Inv[(DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.Parts_online.bit.ac_hub_set_status = Inv_can_mix.ac_hub_data[0].dc_ac_hub_setting.ac_hug_setting.bit.ac_switch;// 171 ac开关状态在此处传

		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,dev_type), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,dev_type),sizeof(Inv_can[num].inv_data[numpack].inv_about.dev_type));// 110~115  ASCII 机型
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,dev_sn), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,dev_sn),sizeof(Inv_can[num].inv_data[numpack].inv_about.dev_sn));// 116~119  设备唯一识别SN码
		iot_sleep_ctx_restore_mod_reg00100(&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1);

		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.d400s_type[0], (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_common_info.d400s_type[0],(offsetof(d400s_common_info_struct ,d400s_sn) - offsetof(d400s_common_info_struct,d400s_type)));// 15500~15509
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.d400s_sn[0], (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_common_info.d400s_sn[0],(offsetof(d400s_common_info_struct,battery_type) - offsetof(d400s_common_info_struct,d400s_sn)));// 15500~15509
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.energy_line, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_common_info.energy_line,sizeof(energy_line_t));//能量流动条   15514
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.battery_type, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_common_info.battery_type,sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.battery_type));//输入电池类型 1:12V铅酸电池 2:24V铅酸电池 15515
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.fault_charger1, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_common_info.fault_charger1,sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.fault_charger1)); // charger故障      15516
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.fault_dcdc, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_common_info.fault_dcdc,sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.fault_dcdc)); // DCDC故障      15517
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.proctect_dcdc, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_common_info.proctect_dcdc,sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.proctect_dcdc)); // DC保护      15518
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.ctrl_mode, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_common_info.ctrl_mode,sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.ctrl_mode)); // DC控制模式      15526
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.total_input_power, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_common_info.total_input_power,sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.total_input_power)); // 所有DC通道总的进入功率 15527~15528
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.total_output_power, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_common_info.total_output_power,sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.total_output_power)); // 所有DC通道总的输出功率 15529~15530
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.dc_info[0], (uint8_t *)&(Inv_can[num].d400s_data[numpack].d400s_common_info.dc_info[0]),sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.dc_info));  //dc 电压 、电流、功率     15531~15554
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.TotalInputEnergy, (uint8_t *)&(Inv_can[num].d400s_data[numpack].d400s_common_info.TotalInputEnergy),sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.TotalInputEnergy));   //电量 0.1kwh 15555~15556
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.energy_info[0], (uint8_t *)&(Inv_can[num].d400s_data[numpack].d400s_common_info.energy_info[0]),sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.energy_info));  // 面向双向DC口定义，进出DCDC模块的能量信息 15557~15580
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.dcdc_SoftwareType, (uint8_t *)&(Inv_can[num].d400s_data[numpack].d400s_common_info.dcdc_SoftwareType),sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.dcdc_SoftwareType));   //软件版本号 例：1001.11； 填充值：100111 15582~15583
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.dcdc_SoftwareVersion, (uint8_t *)&(Inv_can[num].d400s_data[numpack].d400s_common_info.dcdc_SoftwareVersion),sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.dcdc_SoftwareVersion));   //软件版本号 例：1001.11； 填充值：100111 15582~15583

		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15600_D400s_set.charger_set, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_charger_set.charger_set,sizeof(Inv_can[num].d400s_data[numpack].d400s_charger_set.charger_set));//  设置 15600
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15600_D400s_set.dc_val_set[0], (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_charger_set.dc_val_set[0],sizeof(Inv_can[num].d400s_data[numpack].d400s_charger_set.dc_val_set));//电流电压设置 15601~15612
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15600_D400s_set.memory_val_set, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_charger_set.memory_val_set,sizeof(Inv_can[num].d400s_data[numpack].d400s_charger_set.memory_val_set));//  //15613 dc记忆开关模式设置
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15600_D400s_set.mode2_set, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_charger_set.mode2_set,sizeof(Inv_can[num].d400s_data[numpack].d400s_charger_set.mode2_set));//15614 dc充电模式设置1
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15600_D400s_set.mode3_set, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_charger_set.mode3_set,sizeof(Inv_can[num].d400s_data[numpack].d400s_charger_set.mode3_set));//15615 dc充电模式设置2
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15600_D400s_set.batteryCapacity_L, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_charger_set.batteryCapacity_L,sizeof(Inv_can[num].d400s_data[numpack].d400s_charger_set.batteryCapacity_L));//15616 铅酸电池容量 0.1AH
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15600_D400s_set.batteryCapacity_H, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_charger_set.batteryCapacity_H,sizeof(Inv_can[num].d400s_data[numpack].d400s_charger_set.batteryCapacity_H));//15617 铅酸电池容量 0.1AH
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15600_D400s_set.battery_Type, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_charger_set.battery_Type,sizeof(Inv_can[num].d400s_data[numpack].d400s_charger_set.battery_Type));//15618 电池类型
		memcpy((uint8_t *)Inv[DEV_MAIN_NODE_MAX].mod_reg15600_D400s_set.dc_Power_Set, (uint8_t *)Inv_can[num].d400s_data[numpack].d400s_charger_set.dc_Power_Set,sizeof(Inv_can[num].d400s_data[numpack].d400s_charger_set.dc_Power_Set));//15619~15623 流入DC为正，流出为负
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15600_D400s_set.dc_Total_Power_Set, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_charger_set.dc_Total_Power_Set,sizeof(Inv_can[num].d400s_data[numpack].d400s_charger_set.dc_Total_Power_Set));//15624
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15600_D400s_set.mode4_set, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_charger_set.mode4_set,sizeof(Inv_can[num].d400s_data[numpack].d400s_charger_set.mode4_set));//15625

		// modbus 15700
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15700_Dc_Hub_info, (uint8_t *)&Inv_can[num].dc_hub_data[numpack].dc_hub_info,(offsetof(dc_hub_info_struct,rw_cmd) - offsetof(dc_hub_info_struct,dc_hub_type)));// 15700~15749

		// modbus 15750 dc、ac设置区 //testwx 底层Inv_can_mix.ac_hub_data[0].ac_hub_info
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15750_Dc_Ac_Hub_set, (uint8_t *)&Inv_can_mix.dc_ac_hub_setting,(offsetof(dc_ac_hub_set,rw_cmd) - offsetof(dc_ac_hub_set,dc_hug_setting)));// 15700~15749

		// modbus 15800
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg15800_Ac_Hub_info, (uint8_t *)&Inv_can_mix.ac_hub_data[0].ac_hub_info,(offsetof(ac_hub_info_struct,rw_cmd) - offsetof(ac_hub_info_struct,ac_hub_type)));// 15800~15850

		// modbus 40000
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg40000_transparent, (uint8_t *)&Inv_can[num].inv_data[numpack].auth_param,(offsetof(auth_struct,rw_cmd) - offsetof(auth_struct,param)));

		// printf("Inv_can[num].inv_data[numpack].auth_param = \n");
		// esp_log_buffer_hex(TAG, (uint8_t *)&Inv_can[num].inv_data[numpack].auth_param, sizeof(auth_struct));

		// printf("Inv_can_mix.ac_hub_data[0].ac_hub_info.ac_hub_sn = \n");
		// esp_log_buffer_hex(TAG, (uint8_t *)&Inv_can_mix.ac_hub_data[0].ac_hub_info.ac_hub_sn, sizeof(Inv_can_mix.ac_hub_data[0].ac_hub_info.ac_hub_sn));

//////////////////



	for( num = 0;num < (DEV_MAIN_NODE_MAX);num++)//
	{
//单组逆变器的PACK汇总
		// if(type == 0x50) //pack广播消息，主包汇总


		for(numpack =0;numpack < INV_MAX_NUM;numpack++)//
		{
			memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,inv_num), (uint8_t *)&Inv_can[num].inv_data[INV_MAX_NUM].inv_base + offsetof(inv_base_struct,inv_num),sizeof(Inv_can[num].inv_data[INV_MAX_NUM].inv_base.inv_num));// 120,单系统汇总汇总

		    if(type == 0x11)
		    {
//		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,inv_num), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,inv_num),sizeof(Inv_can[num].inv_data[numpack].inv_base.inv_num));// 120
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,inv_online), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,inv_online),sizeof(Inv_can[num].inv_data[numpack].inv_base.inv_online));// 121
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,inv_power_rang), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,inv_power_rang),sizeof(Inv_can[num].inv_data[numpack].inv_base.inv_power_rang));// 122

	            memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,inv_power_rang), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,inv_power_rang),sizeof(Inv_can[num].inv_data[numpack].inv_base.inv_power_rang));// 1111(单机)

		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,line_event), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,line_event),sizeof(Inv_can[num].inv_data[numpack].inv_base.line_event));// 123
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,ctrl_status), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,ctrl_status),sizeof(Inv_can[num].inv_data[numpack].inv_base.ctrl_status));// 124
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,GridandMachineSOC), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,grid_par_soc),sizeof(Inv_can[num].inv_data[numpack].inv_base.grid_par_soc));// 125
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,alarm), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,alarm),sizeof(Inv_can[num].inv_data[numpack].inv_base.alarm));// 126~129
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,fault), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,fault),sizeof(Inv_can[num].inv_data[numpack].inv_base.fault));// 133~136 故障信息
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,fault[4]), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,fault5),sizeof(Inv_can[num].inv_data[numpack].inv_base.fault5));// 137 故障信息
				//CAN_SetAlarmStateFromAcHub(num*INV_MAX_NUM +numpack,0);
				//ESP_LOGI(TAG,"Inv_can[%d].inv_data[%d].inv_base.PvTotalChargingEnergy:%lu",num,numpack,Inv_can[num].inv_data[numpack].inv_base.PvTotalChargingEnergy);
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,DCLoadAllTotalPower), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,DCLoadAllTotalPower),(offsetof(inv_base_struct,fault5) - offsetof(inv_base_struct,DCLoadAllTotalPower)));// 140~159
		        //ESP_LOGI(TAG,"Inv[%d].mod_reg00100_AppPage1.inv_base.PvTotalChargingEnergy:%lu",(num*INV_MAX_NUM +numpack),Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1.PvTotalChargingEnergy);
				// uint16_t  ctrl_chg_mode;  // 160 控制充电模式 0x1a
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,inv_work_state), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,inv_work_state),sizeof(Inv_can[num].inv_data[numpack].inv_base.inv_work_state));// 161
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,PvToACLoadEnergy), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,PvToACLoadEnergy),sizeof(Inv_can[num].inv_data[numpack].inv_base.PvToACLoadEnergy));// 162~163
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,SelfConsumptionPercent), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,SelfConsumptionPercent),sizeof(Inv_can[num].inv_data[numpack].inv_base.SelfConsumptionPercent));// 164
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,PVToACloadPower), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,PVToACloadPower),sizeof(Inv_can[num].inv_data[numpack].inv_base.PVToACloadPower));// 165~166
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,Inv_Flag), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_announce + offsetof(inv_announce_struct,status_flags),sizeof(Inv_can[num].inv_data[numpack].inv_announce.status_flags));// 188
				Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1.SwitchMemoryState = Inv_can[num].inv_data[numpack].inv_base.switch_memory_state;// 191

		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,Rated_Voltage), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,rated_Voltage),sizeof(Inv_can[num].inv_data[numpack].inv_about.rated_Voltage));// 169~170
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,Rated_Frequency), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,rated_Frequency),sizeof(Inv_can[num].inv_data[numpack].inv_about.rated_Frequency));// 171~172


		    }
		    else if(type == 0x12)
		    {
				// 0x11、0x12都有相同的单机能量线的信息，但0x12轮询速度太慢，选择启用0x11的能量线信息
		        // memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,line_event), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_base + offsetof(inv_base_struct,line_event),sizeof(Inv_can[num].inv_data[numpack].inv_base.line_event));// 123
		    }
		    else if(type == 0x13)
		    {
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,dev_type), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,dev_type),sizeof(Inv_can[num].inv_data[numpack].inv_about.dev_type));// 110~115  ASCII 机型
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,dev_sn), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,dev_sn),sizeof(Inv_can[num].inv_data[numpack].inv_about.dev_sn));// 116~119  设备唯一识别SN码
				iot_sleep_ctx_restore_mod_reg00100(&Inv[num*INV_MAX_NUM +numpack].mod_reg00100_AppPage1);

	            memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvType), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,dev_type),sizeof(Inv_can[num].inv_data[numpack].inv_about.dev_type));// 1101~1106  ASCII 机型(单个)
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvSN), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,dev_sn),sizeof(Inv_can[num].inv_data[numpack].inv_about.dev_sn));// 1107~1110  设备唯一识别SN码(单个)

		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,DevVoltageLable), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,voltage_lable),sizeof(Inv_can[num].inv_data[numpack].inv_about.voltage_lable));// 1149
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,WorkTimeNumber), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,time_area_num),sizeof(Inv_can[num].inv_data[numpack].inv_about.time_area_num));// 1148
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,software_total), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,software_total),sizeof(Inv_can[num].inv_data[numpack].inv_about.software_total));// 1112
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvChgLimitL1), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,inv_chg_limit_L1),sizeof(Inv_can[num].inv_data[numpack].inv_about.inv_chg_limit_L1));// 1155
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvChgLimitL2), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,inv_chg_limit_L2),sizeof(Inv_can[num].inv_data[numpack].inv_about.inv_chg_limit_L2));// 1156
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvChgLimitL3), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,inv_chg_limit_L3),sizeof(Inv_can[num].inv_data[numpack].inv_about.inv_chg_limit_L3));// 1157
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvDisgLimitL1), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,inv_disg_limit_L1),sizeof(Inv_can[num].inv_data[numpack].inv_about.inv_disg_limit_L1));// 1158
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvDisgLimitL2), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,inv_disg_limit_L2),sizeof(Inv_can[num].inv_data[numpack].inv_about.inv_disg_limit_L2));// 1159
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvDisgLimitL3), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,inv_disg_limit_L3),sizeof(Inv_can[num].inv_data[numpack].inv_about.inv_disg_limit_L3));// 1160
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,machineL1AcInputRatedCurrent), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,ac_input_rated_current_L1),sizeof(Inv_can[num].inv_data[numpack].inv_about.ac_input_rated_current_L1));// 1161
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,machineL2AcInputRatedCurrent), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,ac_input_rated_current_L2),sizeof(Inv_can[num].inv_data[numpack].inv_about.ac_input_rated_current_L2));// 1162
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,machineL3AcInputRatedCurrent), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,ac_input_rated_current_L3),sizeof(Inv_can[num].inv_data[numpack].inv_about.ac_input_rated_current_L3));// 1163
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,machineL1AcOutputRatedCurrent), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,ac_input_rated_current_L1),sizeof(Inv_can[num].inv_data[numpack].inv_about.ac_output_rated_current_L1));// 1164
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,machineL2AcOutputRatedCurrent), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,ac_input_rated_current_L2),sizeof(Inv_can[num].inv_data[numpack].inv_about.ac_output_rated_current_L2));// 1165
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,machineL3AcOutputRatedCurrent), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,ac_input_rated_current_L3),sizeof(Inv_can[num].inv_data[numpack].inv_about.ac_output_rated_current_L3));// 1166
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,gridL1AcOutputRatedCurrent), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,grid_ac_output_rated_current_L1),sizeof(Inv_can[num].inv_data[numpack].inv_about.grid_ac_output_rated_current_L1));// 1167
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,gridL2AcOutputRatedCurrent), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,grid_ac_output_rated_current_L2),sizeof(Inv_can[num].inv_data[numpack].inv_about.grid_ac_output_rated_current_L2));// 1168
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,gridL3AcOutputRatedCurrent), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_about + offsetof(inv_about_struct,grid_ac_output_rated_current_L3),sizeof(Inv_can[num].inv_data[numpack].inv_about.grid_ac_output_rated_current_L3));// 1169
			// 1113~1130
	            memset((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base.soft, 0,
	                   sizeof(Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base.soft));
	            memset((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base.soft_b, 0,
	                   sizeof(Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base.soft_b));
	            for(int i = 0;i < 6;i++)
	            {
	                Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base.soft[i].type =
	                    Inv_can[num].inv_data[numpack].inv_about.soft[i].type;
	                Inv[num*INV_MAX_NUM +numpack].mod_reg01100_Inv_base.soft[i].version =
	                    Inv_can[num].inv_data[numpack].inv_about.soft[i].version;
				}
		    }
		    else if(type == 0x14)
		    {
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01200_Inv_pv + offsetof(MOD_STRUCT_reg01200,total_chg_power), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_pv + offsetof(inv_pv_struct,total_chg_power),(offsetof(inv_pv_struct,pv_number) - offsetof(inv_pv_struct,total_chg_power)));// 1200~1203
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01200_Inv_pv + offsetof(MOD_STRUCT_reg01200,pv_number), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_pv + offsetof(inv_pv_struct,pv_number),sizeof(Inv_can[num].inv_data[numpack].inv_pv.pv_number));// 1209
		        //memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01200_Inv_pv + offsetof(MOD_STRUCT_reg01200,pv_detail), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_pv + offsetof(inv_pv_struct,pv_detail),sizeof(Inv_can[num].inv_data[numpack].inv_pv.pv_detail));// 1210~1289
		        for(int i = 0;i < 10;i++)// 1210~1289
		        {
		            memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01200_Inv_pv.pv_detail[i].status, (uint8_t *)&Inv_can[num].inv_data[numpack].inv_pv.pv_detail[i].status,sizeof(Inv_can[num].inv_data[numpack].inv_pv.pv_detail[i].status));
		            memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01200_Inv_pv.pv_detail[i].input_type, (uint8_t *)&Inv_can[num].inv_data[numpack].inv_pv.pv_detail[i].input_type,sizeof(Inv_can[num].inv_data[numpack].inv_pv.pv_detail[i].input_type));
		            memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01200_Inv_pv.pv_detail[i].input_power, (uint8_t *)&Inv_can[num].inv_data[numpack].inv_pv.pv_detail[i].input_power,sizeof(Inv_can[num].inv_data[numpack].inv_pv.pv_detail[i].input_power));
		            memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01200_Inv_pv.pv_detail[i].input_voltage, (uint8_t *)&Inv_can[num].inv_data[numpack].inv_pv.pv_detail[i].input_voltage,sizeof(Inv_can[num].inv_data[numpack].inv_pv.pv_detail[i].input_voltage));
		            memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01200_Inv_pv.pv_detail[i].input_current, (uint8_t *)&Inv_can[num].inv_data[numpack].inv_pv.pv_detail[i].input_current,sizeof(Inv_can[num].inv_data[numpack].inv_pv.pv_detail[i].input_current));
		        }
		    }
		    else if(type == 0x15)
		    {
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01300_Inv_grid + offsetof(MOD_STRUCT_reg01300,freq), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_grid + offsetof(inv_grid_struct,freq),(offsetof(inv_grid_struct,grid_phase_number) - offsetof(inv_grid_struct,freq)));// 1300~1306
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01300_Inv_grid + offsetof(MOD_STRUCT_reg01300,grid_phase_number), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_grid + offsetof(inv_grid_struct,grid_phase_number),sizeof(Inv_can[num].inv_data[numpack].inv_grid.grid_phase_number));// 1312
		        for(int i = 0;i < 3;i++)// 1313~1330
		        {
		            memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01300_Inv_grid.grid_detail[i].input_power, (uint8_t *)&Inv_can[num].inv_data[numpack].inv_grid.grid_detail[i].input_power,sizeof(Inv_can[num].inv_data[numpack].inv_grid.grid_detail[i].input_power));
		            memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01300_Inv_grid.grid_detail[i].input_voltage, (uint8_t *)&Inv_can[num].inv_data[numpack].inv_grid.grid_detail[i].input_voltage,sizeof(Inv_can[num].inv_data[numpack].inv_grid.grid_detail[i].input_voltage));
		            memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01300_Inv_grid.grid_detail[i].input_current, (uint8_t *)&Inv_can[num].inv_data[numpack].inv_grid.grid_detail[i].input_current,sizeof(Inv_can[num].inv_data[numpack].inv_grid.grid_detail[i].input_current));
		        }
		    }
		    else if(type == 0x16)
		    {
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01400_Inv_load + offsetof(MOD_STRUCT_reg01400,total_dc_load_power), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_load + offsetof(inv_load_struct,total_dc_load_power),(offsetof(inv_load_struct,total_ac_load_power) - offsetof(inv_load_struct,total_dc_load_power)));// 1400~1409
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01400_Inv_load + offsetof(MOD_STRUCT_reg01400,total_ac_load_power), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_load + offsetof(inv_load_struct,total_ac_load_power),(offsetof(inv_load_struct,ac_phase_number) - offsetof(inv_load_struct,total_ac_load_power)));// 1420~1423
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01400_Inv_load + offsetof(MOD_STRUCT_reg01400,ac_phase_number), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_load + offsetof(inv_load_struct,ac_phase_number),sizeof(Inv_can[num].inv_data[numpack].inv_load.ac_phase_number));// 1429
		        for(int i = 0;i < 3;i++)// 1430~1447
		        {
		            memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01400_Inv_load.ac_load[i].load_power, (uint8_t *)&Inv_can[num].inv_data[numpack].inv_load.ac_load[i].load_power,sizeof(Inv_can[num].inv_data[numpack].inv_load.ac_load[i].load_power));
		            memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01400_Inv_load.ac_load[i].load_voltage, (uint8_t *)&Inv_can[num].inv_data[numpack].inv_load.ac_load[i].load_voltage,sizeof(Inv_can[num].inv_data[numpack].inv_load.ac_load[i].load_voltage));
		            memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01400_Inv_load.ac_load[i].load_current, (uint8_t *)&Inv_can[num].inv_data[numpack].inv_load.ac_load[i].load_current,sizeof(Inv_can[num].inv_data[numpack].inv_load.ac_load[i].load_current));
		        }
		    }
		    else if(type == 0x17)
		    {
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01500_Inv_inv + offsetof(MOD_STRUCT_reg01500,freq), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_data + offsetof(inv_data_struct,freq),sizeof(Inv_can[num].inv_data[numpack].inv_data.freq));// 1500
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01500_Inv_inv + offsetof(MOD_STRUCT_reg01500,total_energy), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_data + offsetof(inv_data_struct,total_energy),sizeof(Inv_can[num].inv_data[numpack].inv_data.total_energy));// 1501~1502 逆变的总能量
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01500_Inv_inv + offsetof(MOD_STRUCT_reg01500,phase_number), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_data + offsetof(inv_data_struct,phase_number),sizeof(Inv_can[num].inv_data[numpack].inv_data.phase_number));// 1508 相位数量 最多3相

				for(int i = 0;i < 3;i++)// 1509~1529
				{
					Inv[num*INV_MAX_NUM +numpack].mod_reg01500_Inv_inv.inv_detail[i].work_status = Inv_can[num].inv_data[numpack].inv_data.inv_detail[i].work_status;// 1509
					Inv[num*INV_MAX_NUM +numpack].mod_reg01500_Inv_inv.inv_detail[i].power = Inv_can[num].inv_data[numpack].inv_data.inv_detail[i].power;// 1510
					Inv[num*INV_MAX_NUM +numpack].mod_reg01500_Inv_inv.inv_detail[i].voltage = Inv_can[num].inv_data[numpack].inv_data.inv_detail[i].voltage;// 1511
					Inv[num*INV_MAX_NUM +numpack].mod_reg01500_Inv_inv.inv_detail[i].current = Inv_can[num].inv_data[numpack].inv_data.inv_detail[i].current;// 1512
				}
		        // memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01500_Inv_inv + offsetof(MOD_STRUCT_reg01500,inv_detail), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_data + offsetof(inv_data_struct,inv_detail),sizeof(Inv_can[num].inv_data[numpack].inv_data.inv_detail));// 1509~1529 每相详细信息 最多3相
		    	// ESP_LOGW(TAG,"Inv_can[%d].inv_data[%d].inv_data.inv_detail[0].power:%u",num,numpack,Inv_can[num].inv_data[numpack].inv_data.inv_detail[0].power);
			}
		    else if(type == 0x18)
		    {

			}
		    else if(type == 0x19)
		    {
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg01600_Inv_generator + offsetof(MOD_STRUCT_reg01600,total_energy), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_gen + offsetof(inv_gen_struct,total_energy),(offsetof(inv_gen_struct,rw_cmd) - offsetof(inv_gen_struct,total_energy)));// 1600~1604
		    }
		    else if(type == 0x1a)// 可写
		    {
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,mon), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,time1),(offsetof(inv_set00_struct,work_mode) - offsetof(inv_set00_struct,time1)));// 2000~2004
				Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set.SetTimeZone.all =SetData.dev_info_t.SetTimeZone.all;// SetData_Can.dev_info_t2.inv_set00.res;// 2004
				// RTC_Valid_Check(num*INV_MAX_NUM +numpack);

				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,work_mode), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,work_mode),sizeof(Inv_can[num].inv_data[numpack].inv_set00.work_mode));// 2005
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ctrl),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl));// 2006
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_led), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ctrl_led),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_led));// 2007
//		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_meter), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ctrl_meter),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_meter));//  2008，windy实际modbus未定义，买电开关需更改至1b区
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_pv), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ctrl_pv),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_pv));// 2009
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_inv), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ctrl_inv),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_inv));// 2010
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_ac), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ctrl_ac),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_ac));// 2011
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dc), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ctrl_dc),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_dc));// 2012
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_poweron), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ctrl_poweron),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_poweron));// 2013
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dc_eco), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ctrl_dc_eco),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_dc_eco));// 2014
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dc_eco_time), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ctrl_dc_eco_time),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_dc_eco_time));// 2015
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,eco_dc_power_value), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,eco_dc_power_value),sizeof(Inv_can[num].inv_data[numpack].inv_set00.eco_dc_power_value));// 2016
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_ac_eco), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ctrl_ac_eco),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_ac_eco));// 2017
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_ac_eco_time), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ctrl_ac_eco_time),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_ac_eco_time));// 2018
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,eco_ac_power_value), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,eco_ac_power_value),sizeof(Inv_can[num].inv_data[numpack].inv_set00.eco_ac_power_value));// 2019
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_chg_mode), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ctrl_chg_mode),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_chg_mode));// 2020
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_super_power), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ctrl_super_power),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_super_power));// 2021
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_low_cap_pct), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ctrl_low_cap_pct),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_low_cap_pct));// 2022
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_high_cap_pct), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ctrl_high_cap_pct),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_high_cap_pct));// 2023
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_inv_mode), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ctrl_inv_mode),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_inv_mode));// 2024
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dev_id), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ctrl_dev_id),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_dev_id));// 2025
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_all_energy_type), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ctrl_all_energy_type),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_all_energy_type));// 2026
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_now_energy_type), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ctrl_now_energy_type),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_now_energy_type));// 2027
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_log_page), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ctrl_log_page),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_log_page));// 2028
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_time_area), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ctrl_time_area),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_time_area));// 2029
		        for(int i = 0;i < 10;i++)// 2030~2059
		        {
		            memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set.ctrl_time[i].lable, (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00.ctrl_time[i].lable,sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_time[i].lable));
		            memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set.ctrl_time[i].start, (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00.ctrl_time[i].start,sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_time[i].start));
		            memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set.ctrl_time[i].end, (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00.ctrl_time[i].end,sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_time[i].end));
		        }
		        for(int i = 0;i < 6;i++)// 2060~2065
		        {
		            memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set.ctrl_PvType[i], (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00.ctrl_PvType[i],sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_PvType[i]));
		        }
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_alarm_voice), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ctrl_alarm_voice),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ctrl_alarm_voice));// 2066
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_lcd_active_time), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,setLcdActiveTime),sizeof(Inv_can[num].inv_data[numpack].inv_set00.setLcdActiveTime));// 2067
				// 2068~2071 对380来讲没有
				//2072~2076
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,self_config),
					(uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,self_config),sizeof(Inv_can[num].inv_data[numpack].inv_set00.self_config));// 2072
				// memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,on_off_set),
				// 	(uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,on_off_set),sizeof(Inv_can[num].inv_data[numpack].inv_set00.on_off_set));// 2073
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,self_config), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,self_config),sizeof(Inv_can[num].inv_data[numpack].inv_set00.self_config));// 2072
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,remoteSet), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,remoteSet),sizeof(Inv_can[num].inv_data[numpack].inv_set00.remoteSet));// 2073
		    	Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set.remoteSoc=Inv_can[num].inv_data[numpack].inv_set00.remoteSoc;	//2074
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ownerShip), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ownerShip),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ownerShip));// 2075
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,LevelSwitch), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,LevelSwitch),sizeof(Inv_can[num].inv_data[numpack].inv_set00.LevelSwitch));// 2076

				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,sleepRemainTime), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,sleepRemainTime),sizeof(Inv_can[num].inv_data[numpack].inv_set00.sleepRemainTime));// 2077
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ledColorSet), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ledColorSet),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ledColorSet));// 2078
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,remote_set_power), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,remote_set_power),sizeof(Inv_can[num].inv_data[numpack].inv_set00.remote_set_power));// 2079
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,pack_set_show), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,pack_set_show),sizeof(Inv_can[num].inv_data[numpack].inv_set00.pack_set_show));// 2080
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,inv_set_show), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,inv_set_show),sizeof(Inv_can[num].inv_data[numpack].inv_set00.inv_set_show));// 2081
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,dcdc_set_show), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,dcdc_set_show),sizeof(Inv_can[num].inv_data[numpack].inv_set00.dcdc_set_show));// 2082
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,soc_max_ownership_set), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,soc_max_ownership_set),sizeof(Inv_can[num].inv_data[numpack].inv_set00.soc_max_ownership_set));// 2083
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,pv_senior_set), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,pv_senior_set),sizeof(Inv_can[num].inv_data[numpack].inv_set00.pv_senior_set));// 2084
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,DC_output), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,dc_output),sizeof(Inv_can[num].inv_data[numpack].inv_set00.dc_output));// 2085
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Regulatory_set), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,Regulatory_set),sizeof(Inv_can[num].inv_data[numpack].inv_set00.Regulatory_set));// 2086
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Cycle_capacity), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,Cycle_capacity),sizeof(Inv_can[num].inv_data[numpack].inv_set00.Cycle_capacity));// 2087
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Cycle_max_capacity), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,Cycle_max_capacity),sizeof(Inv_can[num].inv_data[numpack].inv_set00.Cycle_max_capacity));// 2088
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Effective_time_ym), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,Effective_time_mon),sizeof(Inv_can[num].inv_data[numpack].inv_set00.Effective_time_mon) + sizeof(Inv_can[num].inv_data[numpack].inv_set00.Effective_time_year));// 2089
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Effective_time_dh), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,Effective_time_hour),sizeof(Inv_can[num].inv_data[numpack].inv_set00.Effective_time_hour) + sizeof(Inv_can[num].inv_data[numpack].inv_set00.Effective_time_day));// 2090
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Effective_time_ms), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,Effective_time_sec),sizeof(Inv_can[num].inv_data[numpack].inv_set00.Effective_time_sec) + sizeof(Inv_can[num].inv_data[numpack].inv_set00.Effective_time_min));// 2091
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ECO_status), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,ECO_status),sizeof(Inv_can[num].inv_data[numpack].inv_set00.ECO_status));// 2092
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,set_AC_branch), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,set_AC_branch),sizeof(Inv_can[num].inv_data[numpack].inv_set00.set_AC_branch));// 2093
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,set_DC_branch), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set00 + offsetof(inv_set00_struct,set_DC_branch),sizeof(Inv_can[num].inv_data[numpack].inv_set00.set_DC_branch));// 2094
			}
		    else if(type == 0x1b)// 可写
		    {
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,password), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,password),sizeof(Inv_can[num].inv_data[numpack].inv_set01.password));// 2200~2203
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_reset_factory), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,ctrl_reset_factory),sizeof(Inv_can[num].inv_data[numpack].inv_set01.ctrl_reset_factory));// 2206
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_grid), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,ctrl_grid),sizeof(Inv_can[num].inv_data[numpack].inv_set01.ctrl_grid));// 2207
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_feedback), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,ctrl_feedback),sizeof(Inv_can[num].inv_data[numpack].inv_set01.ctrl_feedback));// 2208
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_output_inv_volt), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,ctrl_output_inv_volt),(offsetof(inv_set01_struct,ctrl_user_area) - offsetof(inv_set01_struct,ctrl_output_inv_volt)));// 2209~2217
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_user_area), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,ctrl_user_area),sizeof(Inv_can[num].inv_data[numpack].inv_set01.ctrl_user_area));// 2218
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_pv_paralle), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,ctrl_pv_paralle),sizeof(Inv_can[num].inv_data[numpack].inv_set01.ctrl_pv_paralle));// 2219~2224
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_grid_plus), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,ctrl_grid_plus),sizeof(Inv_can[num].inv_data[numpack].inv_set01.ctrl_grid_plus));// 2225
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_save_power_state), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,ctrl_save_power_state),sizeof(Inv_can[num].inv_data[numpack].inv_set01.ctrl_save_power_state));// 2226
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_meter_enable), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,ctrl_meter_enable),sizeof(Inv_can[num].inv_data[numpack].inv_set01.ctrl_meter_enable));// 2227
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_meter_select), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,ctrl_meter_select),sizeof(Inv_can[num].inv_data[numpack].inv_set01.ctrl_meter_select));// 2228
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_Inv_Multi_enable), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,ctrl_Inv_Multi_enable),sizeof(Inv_can[num].inv_data[numpack].inv_set01.ctrl_Inv_Multi_enable));// 2229
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_Inv_addr_Set), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,ctrl_Inv_addr_Set),sizeof(Inv_can[num].inv_data[numpack].inv_set01.ctrl_Inv_addr_Set));// 2230
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ct_test), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,ct_test),(offsetof(inv_set01_struct,ctrl_mix2) - offsetof(inv_set01_struct,ct_test)));// 2231~2232

				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_mix2), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,ctrl_mix2),sizeof(Inv_can[num].inv_data[numpack].inv_set01.ctrl_mix2));// 2242
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ChargingPile_SET), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,ChargingPile_SET),sizeof(Inv_can[num].inv_data[numpack].inv_set01.ChargingPile_SET));// 2243
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ct_ratio), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,ct_ratio),sizeof(Inv_can[num].inv_data[numpack].inv_set01.ct_ratio));// 2244
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,GenSet), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,GenSet),(offsetof(inv_set01_struct,Undervoltage_protection) - offsetof(inv_set01_struct,GenSet)));// 2246~2257	res1
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Undervoltage_protection), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,Undervoltage_protection),sizeof(Inv_can[num].inv_data[numpack].inv_set01.Undervoltage_protection));// 2258
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Undervoltage_protection_time), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,Undervoltage_protection_time),sizeof(Inv_can[num].inv_data[numpack].inv_set01.Undervoltage_protection_time));// 2259
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Highvoltage_protection), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,Highvoltage_protection),sizeof(Inv_can[num].inv_data[numpack].inv_set01.Highvoltage_protection));// 2260
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Highvoltage_protection_time), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,Highvoltage_protection_time),sizeof(Inv_can[num].inv_data[numpack].inv_set01.Highvoltage_protection_time));// 2261
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Underfrequency_protection), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,Underfrequency_protection),sizeof(Inv_can[num].inv_data[numpack].inv_set01.Underfrequency_protection));// 2262
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Underfrequency_protection_time), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,Underfrequency_protection_time),sizeof(Inv_can[num].inv_data[numpack].inv_set01.Underfrequency_protection_time));// 2263
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Overvoltage_protection), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,Overvoltage_protection),sizeof(Inv_can[num].inv_data[numpack].inv_set01.Overvoltage_protection));// 2264
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Overvoltage_protection_time), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,Overvoltage_protection_time),sizeof(Inv_can[num].inv_data[numpack].inv_set01.Overvoltage_protection_time));// 2265
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,SetCtrlPv), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,setting_pv),sizeof(Inv_can[num].inv_data[numpack].inv_set01.setting_pv));// 2269
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Phase_set), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,Phase_set),sizeof(Inv_can[num].inv_data[numpack].inv_set01.Phase_set));// 2270
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,DCHUB_set), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,DCHUB_set),sizeof(Inv_can[num].inv_data[numpack].inv_set01.DCHUB_set));// 2271
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,SetGridMaxCurrent_in), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,SetGridMaxCurrent_in),sizeof(Inv_can[num].inv_data[numpack].inv_set01.SetGridMaxCurrent_in));// 2272
		    	memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Func_Set), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,Func_Set),sizeof(Inv_can[num].inv_data[numpack].inv_set01.Func_Set));// 2273
				memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,RvSettings), (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set01 + offsetof(inv_set01_struct,HomeCarBat_Set),sizeof(Inv_can[num].inv_data[numpack].inv_set01.HomeCarBat_Set));// 2274
			}
		    else if(type == 0x1c)// 可写
		    {
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02300_Inv_set02_struct, (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set02,(offsetof(inv_set02_struct,rw_cmd) - offsetof(inv_set02_struct,SetGridUV1Value)));// 2300~
		    }
		    else if(type == 0x1d)// 可写
		    {
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg02400_Inv_certification, (uint8_t *)&Inv_can[num].inv_data[numpack].inv_set03,(offsetof(inv_set03_struct,rw_cmd) - offsetof(inv_set03_struct,SetGridEnable)));// 2400~
		    }
		    else if(type == 0x20)//  历史记录
		    {
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg03000_Inv_history, (uint8_t *)&Inv_can[num].inv_data[numpack].inv_log,(offsetof(inv_log_struct,rw_cmd) - offsetof(inv_log_struct,total_page)));// 3000~
		    }
		    else if(type == 0x27)//
		    {
		        memcpy((uint8_t *)&Inv[num*INV_MAX_NUM +numpack].mod_reg40000_transparent, (uint8_t *)&Inv_can[num].inv_data[numpack].auth_param,(offsetof(auth_struct,rw_cmd) - offsetof(auth_struct,param[0])));// 3000~
		    }
			else if(type == 0x41)//  DC_Hub信息区
		    {
		        // memcpy((uint8_t *)&Inv[num*DC_HUB_MAX_NUM +numpack].mod_reg15700_Dc_Hub_info, (uint8_t *)&Inv_can[num].dc_hub_data[numpack].dc_hub_info,(offsetof(dc_hub_info_struct,rw_cmd) - offsetof(dc_hub_info_struct,dc_hub_type)));// 15700~15749
				memcpy((uint8_t *)&Inv[num*DC_HUB_MAX_NUM +numpack].mod_reg15700_Dc_Hub_info, (uint8_t *)&Inv_can[num].dc_hub_data[numpack].dc_hub_info,(offsetof(dc_hub_info_struct,input_power) - offsetof(dc_hub_info_struct,dc_hub_type)));// 15700~15709
				// 15710~15711 来源于主动上报、不从主动轮询获取
				memcpy((uint8_t *)&Inv[num*DC_HUB_MAX_NUM +numpack].mod_reg15700_Dc_Hub_info + offsetof(MOD_STRUCT_reg15700,input_current), (uint8_t *)&Inv_can[num].dc_hub_data[numpack].dc_hub_info.input_current,(offsetof(dc_hub_info_struct,rw_cmd) - offsetof(dc_hub_info_struct,input_current)));// 15712~15749
		    	ESP_LOGI(TAG,"AAA Inv_can[%d].dc_hub_data[%d].dc_hub_info.cig2_output_power:0x%x ",num,numpack,Inv_can[num].dc_hub_data[numpack].dc_hub_info.cig2_output_power);
				ESP_LOGI(TAG,"AAA Inv[%d].mod_reg15700_Dc_Hub_info.cig2_output_power:0x%x ",num*DC_HUB_MAX_NUM +numpack,Inv[num*DC_HUB_MAX_NUM +numpack].mod_reg15700_Dc_Hub_info.cig2_output_power);
			}
			else if(type == 0x42)//  AC_Hub信息区
		    {
		        memcpy((uint8_t *)&Inv[num*AC_HUB_MAX_NUM +numpack].mod_reg15800_Ac_Hub_info, (uint8_t *)&Inv_can_mix.ac_hub_data[numpack].ac_hub_info,(offsetof(ac_hub_info_struct,SoftwareType) - offsetof(ac_hub_info_struct,ac_hub_type)));// 15800~15809
				//achub版本号统一放在judge_inv_min_version处理
				memcpy((uint8_t *)&Inv[num*AC_HUB_MAX_NUM +numpack].mod_reg15800_Ac_Hub_info.ACHUB_safe_code, (uint8_t *)&Inv_can_mix.ac_hub_data[numpack].ac_hub_info.ACHUB_safe_code,sizeof(Inv_can_mix.ac_hub_data[numpack].ac_hub_info.ACHUB_safe_code));// 15813~15816
				// 15817~15822 预留
				memcpy((uint8_t *)&Inv[num*AC_HUB_MAX_NUM +numpack].mod_reg15800_Ac_Hub_info.InvVoltageL1, (uint8_t *)&Inv_can_mix.ac_hub_data[numpack].ac_hub_info.InvVoltageL1,(offsetof(ac_hub_info_struct,Power_load) - offsetof(ac_hub_info_struct,InvVoltageL1)));// 15823~15828
		    	memcpy((uint8_t *)&Inv[num*AC_HUB_MAX_NUM +numpack].mod_reg15800_Ac_Hub_info.Power_grid, (uint8_t *)&Inv_can_mix.ac_hub_data[numpack].ac_hub_info.Power_grid,(offsetof(ac_hub_info_struct,alarm_State) - offsetof(ac_hub_info_struct,Power_grid)));// 15832~15836
			}
			else if(type == 0x48)// DCDC模块通用 信息区
			{
				ESP_LOGI(TAG,"0X48 DCDC COMM INFO UPDATE");
 				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15500_D400s_info.d400s_type[0], (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_common_info.d400s_type[0],(offsetof(d400s_common_info_struct ,d400s_sn) - offsetof(d400s_common_info_struct,d400s_type)));// 15500~15509
				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15500_D400s_info.d400s_sn[0], (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_common_info.d400s_sn[0],(offsetof(d400s_common_info_struct ,battery_type) - offsetof(d400s_common_info_struct,d400s_sn)));// 15500~15509
				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15500_D400s_info.energy_line, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_common_info.energy_line,sizeof(energy_line_t));//能量流动条   15514
				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15500_D400s_info.battery_type, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_common_info.battery_type,sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.battery_type));//输入电池类型 1:12V铅酸电池 2:24V铅酸电池 15515

				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15500_D400s_info.fault_charger1, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_common_info.fault_charger1,sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.fault_charger1)); // charger故障      15516
				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15500_D400s_info.fault_dcdc, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_common_info.fault_dcdc,sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.fault_dcdc)); // DCDC故障      15517
				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15500_D400s_info.proctect_dcdc, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_common_info.proctect_dcdc,sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.proctect_dcdc)); // DC保护      15518
				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15500_D400s_info.ctrl_mode, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_common_info.ctrl_mode,sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.ctrl_mode)); // DC控制模式      15526

				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15500_D400s_info.total_input_power, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_common_info.total_input_power,sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.total_input_power)); // 所有DC通道总的进入功率 15527~15528
				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15500_D400s_info.total_output_power, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_common_info.total_output_power,sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.total_output_power)); // 所有DC通道总的输出功率 15529~15530
				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15500_D400s_info.dc_info[0], (uint8_t *)&(Inv_can[num].d400s_data[numpack].d400s_common_info.dc_info[0]),sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.dc_info));  //dc 电压 、电流、功率     15531~15554
				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15500_D400s_info.TotalInputEnergy, (uint8_t *)&(Inv_can[num].d400s_data[numpack].d400s_common_info.TotalInputEnergy),sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.TotalInputEnergy));   //电量 0.1kwh 15555~15556
				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15500_D400s_info.energy_info[0], (uint8_t *)&(Inv_can[num].d400s_data[numpack].d400s_common_info.energy_info[0]),sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.energy_info));  // 面向双向DC口定义，进出DCDC模块的能量信息 15557~15580
				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15500_D400s_info.dcdc_SoftwareType, (uint8_t *)&(Inv_can[num].d400s_data[numpack].d400s_common_info.dcdc_SoftwareType),sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.dcdc_SoftwareType));   //软件版本号 例：1001.11； 填充值：100111 15582~15583
				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15500_D400s_info.dcdc_SoftwareVersion, (uint8_t *)&(Inv_can[num].d400s_data[numpack].d400s_common_info.dcdc_SoftwareVersion),sizeof(Inv_can[num].d400s_data[numpack].d400s_common_info.dcdc_SoftwareVersion));   //软件版本号 例：1001.11； 填充值：100111 15582~15583
			}
			else if(type == 0x49)//DCDC模块通用/CHARGER 1 设置区
			{
				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15600_D400s_set.charger_set, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_charger_set.charger_set,sizeof(Inv_can[num].d400s_data[numpack].d400s_charger_set.charger_set));//  设置 15600
				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15600_D400s_set.dc_val_set[0], (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_charger_set.dc_val_set[0],sizeof(Inv_can[num].d400s_data[numpack].d400s_charger_set.dc_val_set));//电流电压设置 15601~15612
				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15600_D400s_set.memory_val_set, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_charger_set.memory_val_set,sizeof(Inv_can[num].d400s_data[numpack].d400s_charger_set.memory_val_set));//  //15613 dc记忆开关模式设置
				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15600_D400s_set.mode2_set, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_charger_set.mode2_set,sizeof(Inv_can[num].d400s_data[numpack].d400s_charger_set.mode2_set));//15614 dc充电模式设置1
				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15600_D400s_set.mode3_set, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_charger_set.mode3_set,sizeof(Inv_can[num].d400s_data[numpack].d400s_charger_set.mode3_set));//15615 dc充电模式设置2
				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15600_D400s_set.batteryCapacity_L, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_charger_set.batteryCapacity_L,sizeof(Inv_can[num].d400s_data[numpack].d400s_charger_set.batteryCapacity_L));//15616 铅酸电池容量 0.1AH
				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15600_D400s_set.batteryCapacity_H, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_charger_set.batteryCapacity_H,sizeof(Inv_can[num].d400s_data[numpack].d400s_charger_set.batteryCapacity_H));//15617 铅酸电池容量 0.1AH
				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15600_D400s_set.battery_Type, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_charger_set.battery_Type,sizeof(Inv_can[num].d400s_data[numpack].d400s_charger_set.battery_Type));//15618 电池类型
				memcpy((uint8_t *)Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15600_D400s_set.dc_Power_Set, (uint8_t *)Inv_can[num].d400s_data[numpack].d400s_charger_set.dc_Power_Set,sizeof(Inv_can[num].d400s_data[numpack].d400s_charger_set.dc_Power_Set));//15619~15623 流入DC为正，流出为负
				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15600_D400s_set.dc_Total_Power_Set, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_charger_set.dc_Total_Power_Set,sizeof(Inv_can[num].d400s_data[numpack].d400s_charger_set.dc_Total_Power_Set));//15624
				memcpy((uint8_t *)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg15600_D400s_set.mode4_set, (uint8_t *)&Inv_can[num].d400s_data[numpack].d400s_charger_set.mode4_set,sizeof(Inv_can[num].d400s_data[numpack].d400s_charger_set.mode4_set));//15625
			}
			else if(type == 0x01)
			{
				memcpy((uint8_t*)Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg11000_IOT_info.iot_type,(uint8_t*)Inv_can[num].d400s_data[numpack].iot_can_11000.iot_type,sizeof(Inv_can[num].d400s_data[numpack].iot_can_11000.iot_type));/*11000~11005*/
				Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg11000_IOT_info.iot_sn=Inv_can[num].d400s_data[numpack].iot_can_11000.iot_sn; /*11006~11009*/
				Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg11000_IOT_info.iot_sn=Inv_can[num].d400s_data[numpack].iot_can_11000.iot_sn; /*11010~11001安全码3*/
				Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg11000_IOT_info.software_ver=Inv_can[num].d400s_data[numpack].iot_can_11000.software_ver; /*11014~11015*/
				memcpy((uint8_t*)&Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg11000_IOT_info.link,(uint8_t*)&Inv_can[num].d400s_data[numpack].iot_can_11000.link,sizeof(Inv_can[num].d400s_data[numpack].iot_can_11000.link));
				Inv_D400S[num*D400S_MAX_NUM +numpack].mod_reg11000_IOT_info.Bind_SN=Inv_can[num].d400s_data[numpack].iot_can_11000.Bind_SN;
			}
		}
	}

//////////////////////pack

/////////////////////	单PACK
	for( num = 0;num < (DEV_MAIN_NODE_MAX);num++)//
	{

		for(numpack =0;numpack < PACK_MAX_NUM;numpack++)// PACK_MAX_NUM
		{

	// Pack


            if(type == 0x51)
			{
                // /*6100~6110*/ 0x54

				memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,total_voltage), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,total_voltage),(offsetof(pack_base_struct,soc) - offsetof(pack_base_struct,total_voltage)));// 6111~6112
				memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,soc), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,soc),sizeof(Inv_can[num].pack_data[numpack].pack_base.soc));// 6113
				memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,soh), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,soh),sizeof(Inv_can[num].pack_data[numpack].pack_base.soh));// 6114
				memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,avg_temp), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,avg_temp),sizeof(Inv_can[num].pack_data[numpack].pack_base.avg_temp));// 6115
				memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,min_cell_voltage), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,min_cell_voltage),(offsetof(pack_base_struct,min_cell_index) - offsetof(pack_base_struct,min_cell_voltage)));// 6116~6117
				memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,min_cell_index), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,min_cell_index),sizeof(Inv_can[num].pack_data[numpack].pack_base.min_cell_index));// 6118
				memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,max_cell_index), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,max_cell_index),sizeof(Inv_can[num].pack_data[numpack].pack_base.max_cell_index));// 6119
                memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,min_temp_value), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,min_temp_value),sizeof(Inv_can[num].pack_data[numpack].pack_base.min_temp_value));// 6120
                memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,max_temp_value), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,max_temp_value),sizeof(Inv_can[num].pack_data[numpack].pack_base.max_temp_value));// 6121
                memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,min_temp_index), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,min_temp_index),sizeof(Inv_can[num].pack_data[numpack].pack_base.min_temp_index));// 6122
                memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,max_temp_index), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,max_temp_index),sizeof(Inv_can[num].pack_data[numpack].pack_base.max_temp_index));// 6123
                memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,work_status), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,work_status),sizeof(Inv_can[num].pack_data[numpack].pack_base.work_status));// 6124
                memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,chg_status), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,chg_status),sizeof(Inv_can[num].pack_data[numpack].pack_base.chg_status));// 6125
                // memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,pack_online), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,pack_cap_online),sizeof(Inv_can[num].pack_data[numpack].pack_base.pack_cap_online));// 6127
                memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,relay), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,relay),sizeof(Inv_can[num].pack_data[numpack].pack_base.relay));// 6128
                memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,pack_cap_online), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,pack_cap_online),sizeof(Inv_can[num].pack_data[numpack].pack_base.pack_cap_online));// 6129
                // 6130 pack_canbus_error
                // /*6131~6143*/ 0x52

                memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,protect), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,protect),(offsetof(pack_base_struct,relay) - offsetof(pack_base_struct,protect)));// 6144~6149
                memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,chg_full_time), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,protect),(offsetof(pack_base_struct,allow_max_chg_voltage) - offsetof(pack_base_struct,chg_full_time)));// 6150~6151

				// 6152~6153 在AC380中被用作每种类型的总的电芯、探头数量
                // memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,pack_total_cell), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,pack_total_cell),sizeof(Inv_can[num].pack_data[numpack].pack_base.pack_total_cell));// 6152
                // memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,pack_total_ntc), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,pack_total_ntc),sizeof(Inv_can[num].pack_data[numpack].pack_base.pack_total_ntc));// 6153
				// 6154 PackBMUCnt 在AC380中被用作每种类型的电池包数量(.eg:B300K有3个电池包 若B300K是通过从机地址01读取时 该值为3)
                // 6157
                // memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,pack_outsum_voltage), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,vbus),sizeof(Inv_can[num].pack_data[numpack].pack_base.vbus));// 6158
                memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,allow_max_chg_current), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_base + offsetof(pack_base_struct,allow_max_chg_current),(offsetof(pack_base_struct,protect_status) - offsetof(pack_base_struct,allow_max_chg_current)));// 6160~6161
                // 6173 /*6174~6203*/ 0x54


			}
            else if(type == 0x52)
            {
                memcpy((uint8_t *)&Inv[num].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,PackTotalDsgEnergy), (uint8_t *)&Inv_can[num].pack_data[0].pack_extend + offsetof(pack_extend_struct,total_dsg_energy),sizeof(Inv_can[num].pack_data[0].pack_extend.total_dsg_energy));// 167~168
                // /*6131~6143*/ 0x52
                memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,capacity), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_extend + offsetof(pack_extend_struct,capacity),(offsetof(pack_extend_struct,rw_cmd) - offsetof(pack_extend_struct,capacity)));// 6131~6143
            }
            else if(type == 0x54)
            {
				// 防止对外置电池包的数据进行覆盖干扰，6101~6110、6173、6174的数据在 judge_pack_min_version 处处理
                // memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,type_ascii), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_about + offsetof(pack_about_struct,type_ascii),(offsetof(pack_about_struct,software_total) - offsetof(pack_about_struct,type_ascii)));// 6101~6110
                // memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,software_total), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_about + offsetof(pack_about_struct,software_total),sizeof(Inv_can[num].pack_data[numpack].pack_about.software_total));// 6173
                // // 6174~6203
                // for(int i = 0;i < 10;i++)
                // {
                //     memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each.soft[i].type, (uint8_t *)&Inv_can[num].pack_data[numpack].pack_about.soft[i].type,sizeof(Inv_can[num].pack_data[numpack].pack_about.soft[i].type));
                //     memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg06100_Pack_each.soft[i].version, (uint8_t *)&Inv_can[num].pack_data[numpack].pack_about.soft[i].version,sizeof(Inv_can[num].pack_data[numpack].pack_about.soft[i].version));
                // }
            }
            else if(type == 0x55)// 可写
            {
                // 7000
                memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg07000_Pack_set + offsetof(MOD_STRUCT_reg07000,pack_heat_enable), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_about + offsetof(pack_config_struct,pack_heat_enable),sizeof(Inv_can[num].pack_data[numpack].pack_config.pack_heat_enable));// 7001
                memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg07000_Pack_set + offsetof(MOD_STRUCT_reg07000,ctr_heat_enable), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_about + offsetof(pack_config_struct,ctr_heat_enable),sizeof(Inv_can[num].pack_data[numpack].pack_config.ctr_heat_enable));// 7002
                memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg07000_Pack_set + offsetof(MOD_STRUCT_reg07000,unlock_failed_flags), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_about + offsetof(pack_config_struct,unlock_failed_flags),sizeof(Inv_can[num].pack_data[numpack].pack_config.unlock_failed_flags));// 7003
                memcpy((uint8_t *)&Inv_Pack[num*PACK_MAX_NUM +numpack].mod_reg07000_Pack_set + offsetof(MOD_STRUCT_reg07000,max_parallel_nums), (uint8_t *)&Inv_can[num].pack_data[numpack].pack_about + offsetof(pack_config_struct,max_parallel_nums),sizeof(Inv_can[num].pack_data[numpack].pack_config.max_parallel_nums));// 7004
            }
            else if(type == 0x56)
            {

            }
		}
	}
}

/*------------------------------------------------------------------------------
 Function: CAN_Dev_Ctrl_SetData_Check
 -----------------------------------------------------------------------------*/
/**
inv设置区部分信息（休眠、SOC设置等）同步，

  */
void CAN_Dev_Ctrl_SetData_Check(uint8_t type)
{
	switch(type)
	{
        case INV_TYPE_CONFIG00_1AH: //set
		{
			ESP_LOGI(TAG,"CAN_Dev_Ctrl_SetData_Check ctrl_all:0x%x --0x%x --0x%x",SetData_Can.dev_info_t2.inv_set00.ctrl.all,Inv_can[reals.online_Y_inv_index].inv_data[reals.online_Y_inv_index].inv_set00.ctrl.all,Inv_WR.mod_reg02000_Inv_base_set.ctrl.all);
			if(memcmp( &SetData_Can.dev_info_t2.inv_set00,&Inv_can[reals.online_Y_inv_index].inv_data[reals.online_Y_inv_index].inv_set00,sizeof(inv_set00_struct)))
			{
				//2006寄存器控制事件，EL300只由APP控制，ARM不做控制，不听从ARM控制
				if(SetData_Can.dev_info_t2.inv_set00.ctrl.all!=Inv_can[reals.online_Y_inv_index].inv_data[reals.online_Y_inv_index].inv_set00.ctrl.all)
				{
					Inv_can[reals.online_Y_inv_index].inv_data[reals.online_Y_inv_index].inv_set00.ctrl.all=SetData_Can.dev_info_t2.inv_set00.ctrl.all;
					Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl.all=SetData_Can.dev_info_t2.inv_set00.ctrl.all;
				}
			}
		}
		break;
		default:
		break;
	}
}

	//将ACHUB的报警状态汇总到故障表中
void CAN_SetAlarmStateFromAcHub(uint8_t num,uint8_t unumpack)
{
	uint16_t    alarm[4];       	// 126~129 告警信息
    uint16_t    fault[5];       	// 133~137 故障信息
	memset(alarm,0,sizeof(alarm));
	memset(fault,0,sizeof(fault));
	uint16_t AlarmState= Inv_can_mix.ac_hub_data[unumpack].ac_hub_info.alarm_State.all;
	//ESP_LOGI(TAG,"AlarmState:0x%x Inv_can_mix.ac_hub_data[%d].ac_hub_info.alarm_State.bit.GridWorkSta:%d",AlarmState,unumpack,Inv_can_mix.ac_hub_data[unumpack].ac_hub_info.alarm_State.bit.GridWorkSta);
	//获取逆变器过载bit0 逆变器短路状态bit2 主继电器故障bit12 负载继电器故障状态bit13
	fault[0]|=(uint16_t)(((AlarmState>>7)&0x4)|((AlarmState>>8)&0x1)|((AlarmState<<10)&0x800)|((AlarmState<<12)&0x1000));
	Inv[num].mod_reg00100_AppPage1.fault[0]|=fault[0];
	//获取电网缺相bit1
	fault[1]|=(uint16_t)(AlarmState>>6)&0x2;
	Inv[num].mod_reg00100_AppPage1.fault[1]|=fault[1];
	//获取电网过压bit0 电网过欠压bit1 电网过频bit2 电网欠频bit3 电网振荡bit4
	alarm[0]|=(uint16_t)(AlarmState>>2)&0x1F;
	Inv[num].mod_reg00100_AppPage1.alarm[0]|=alarm[0];
	//获取配件工作异常bit7
	alarm[0]|=(uint16_t)(AlarmState>>4)&0x80;
	Inv[num].mod_reg00100_AppPage1.alarm[0]|=alarm[0];
	//获取控制器过温bit10
	fault[4]|=(uint16_t)(AlarmState&0x400);
	Inv[num].mod_reg00100_AppPage1.fault[4]|=fault[4];
	//或取并机地址重复报警 bit2
	fault[1]|=(uint16_t)(reals.Addr_can_Attr<<1)&0x4;
	Inv[num].mod_reg00100_AppPage1.fault[1]|=fault[1];
	//ESP_LOGW(TAG, "CAN_SetAlarmStateFromAcHub num:%d, unumpack:%d,  fault= 0x%x,alarm:0%x,AlarmState:0x%x", num,unumpack,Inv[num].mod_reg00100_AppPage1.fault[0],Inv[num].mod_reg00100_AppPage1.alarm[0],AlarmState);

	memcpy(&Inv_AcHub.mod_reg00100_AppPage1.fault[0],&fault[0],sizeof(Inv_AcHub.mod_reg00100_AppPage1.fault));
	memcpy(&Inv_AcHub.mod_reg00100_AppPage1.alarm[0],&alarm[0],sizeof(Inv_AcHub.mod_reg00100_AppPage1.alarm));
	//ESP_LOGW(TAG, "CAN_SetAlarmStateFromAcHub Inv_AcHub fault= 0x%x,alarm:0%x,AlarmState:0x%x",Inv_AcHub.mod_reg00100_AppPage1.fault[0],Inv_AcHub.mod_reg00100_AppPage1.alarm[0],AlarmState);
}
