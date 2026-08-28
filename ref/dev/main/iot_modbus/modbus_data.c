/*
 * File : prot_param.c
 * 该文件是智能插座软件工程的一部分
 * 该文件提供modbus数据表注册，并自动执行初始化
 * 自动初始化功能利用__attribute__((constructor))属性实现
 * 需要在编译中添加WHOLE_ARCHIVE选项
 * 
 * 该文件定义了每个modbus表的处理函数，在函数中处理表格读写操作
 * 
 * Change Logs:
 * Date         Author          Notes
 * 2024-03-28   heyinping       初始版本


 constructor在main开始运行之前被调用，destructor在main函数结束后被调用。如果有多个constructor或destructor，可以给每个constructor或destructor赋予优先级，对于constructor，优先级数值越小，运行越早。destructor则相反
 */

#include "modbus_data.h"
#include "modbus_protocol.h"
#include "esp_check.h"
#include "esp_log.h"
#include <string.h>
//#include "cfg_param.h"
//#include "plug_param.h"
//#include "system.h"
//#include "energy.h"


#define TAG "md_data"

#define BIT_COMPARE(new_val, old_val)   (((old_val == CFG_INVALID) && (new_val == CFG_ENABLE)) ||  \
                                        ((old_val != CFG_INVALID) && (new_val != CFG_INVALID) && (new_val != old_val)))


/**
 * @brief 蓝牙设置数据表处理
 * 
 * @param tbl 数据表指针
 * @param cb_data 参数
 * @param priv_data 私有数据
 * @return 成功返回0，失败返回-1
 */
static int ble_set_tbl_handler(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    ble_set_t data = {0};
    md_tbl_t *ptbl = (md_tbl_t *)tbl;
//    cfg_param_t *cfg_param = parameter_get(CFG_PARAM_NAME);
//
//    if (cb_data->is_write)
//    {
//        assert(md_reg_data_get(ptbl->start, (uint16_t*)&data, sizeof(ble_set_t)) == 0);
//        int reg_nums = cb_data->reg_nums;
//        uint16_t data_offset = (cb_data->reg_addr - ptbl->start) * 2;
//
//        while (reg_nums > 0)
//        {
//            switch (data_offset)
//            {
//            case offsetof(ble_set_t, ble_cfg.ble_flag):
//                data_offset += sizeof(data.ble_cfg.ble_flag);
//                reg_nums -= (sizeof(data.ble_cfg.ble_flag) / 2);
//                if (BIT_COMPARE(data.ble_cfg.ble_flag.guest_falg, cfg_param->ble_cfg.ble_flag.guest_falg)) {
//                    cfg_param->ble_cfg.ble_flag.guest_falg = data.ble_cfg.ble_flag.guest_falg;
//                }
//                break;
//            
//            case offsetof(ble_set_t, ble_cfg.ble_pwd):
//                data_offset += sizeof(data.ble_cfg.ble_pwd);
//                reg_nums -= (sizeof(data.ble_cfg.ble_pwd) / 2);
//                memcpy(cfg_param->ble_cfg.ble_pwd, data.ble_cfg.ble_pwd, sizeof(cfg_param->ble_cfg.ble_pwd));
//                break;
//
//            default:
//                ESP_LOGW(TAG, "unknown modbus register: %d", (ptbl->start+data_offset)/2);
//                break;
//            }
//        }
//        
//        assert(parameter_set(CFG_PARAM_NAME, cfg_param) == 0);
//    }
//    else
//    {
//        data.modbus_ver = MODBUS_VERSION;
//        memcpy(&data.ble_cfg, &cfg_param->ble_cfg, sizeof(data.ble_cfg));
//        assert(md_reg_data_set(ptbl->start, (uint16_t*)&data, sizeof(ble_set_t)) == 0);
//    }
    return 0;
}

/**
 * @brief 蓝牙设置数据表注册
 * 
 */
//static void __attribute__((constructor(101))) ble_set_table_register(void)


#if 0
/**
 * @brief ota数据表处理
 * 
 * @param tbl 数据表指针
 * @param cb_data 参数
 * @param priv_data 私有数据
 * @return 成功返回0，失败返回-1
 */
static int ota_data_tbl_handler(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    if (!cb_data->is_write) return 0;

	ota_data_t ota_data;
    md_tbl_t *ptbl = (md_tbl_t *)tbl;
	assert(md_reg_data_get(ptbl->start, (uint16_t*)&ota_data, sizeof(ota_data)) == 0);
	if (ota_data.start == 0) return 0;

	#define SMART_PLUG_FILE_TYPE 17
	if (ota_data.file_type != SMART_PLUG_FILE_TYPE)
	{
		ESP_LOGE(TAG, "ota file type error, type: %d", ota_data.file_type);
		return -1;
	}

    md_priv_data_t *priv = (md_priv_data_t*)priv_data;
    ota_port_t ota_port = {
        .type = priv->ota_type,
        .data_send = priv->ota_response,
    };

	if (fw_upgrade_start(&ota_port) != 0)
    {
		ESP_LOGE(TAG, "ota start failure");
		return -1;
    }

	ota_data.start = 0;
	assert(md_reg_data_set(ptbl->start, (uint16_t*)&ota_data, sizeof(ota_data)) == 0);
    return 0;
}

/**
 * @brief ota数据表注册
 * 
 */
static void __attribute__((constructor(102))) ota_data_table_register(void)
{
    md_add_tbl(&(md_tbl_t){
        .start = OTA_DATA_TBL_START,
        .end = OTA_DATA_TBL_END,
        .is_write = TBL_READ_WRITE,
        .tbl_cb = ota_data_tbl_handler,
    });
}

/**
 * @brief 设备只读数据表处理
 * 
 * @param tbl 数据表指针
 * @param cb_data 参数
 * @param priv_data 私有数据
 * @return 成功返回0，失败返回-1
 */
static int dev_ro_data_tbl_handler(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    if (cb_data->is_write) return -1;
    
    dev_ro_data_t data = {0};
    cfg_param_t *cfg_param = parameter_get(CFG_PARAM_NAME);
    memcpy(&data.dev_type, cfg_param->factory_cfg.dev_type, sizeof(data.dev_type));
    data.dev_sn = cfg_param->factory_cfg.dev_sn;
    data.safe_code = cfg_param->factory_cfg.safe_code;
    data.soft_ver = SYS_FW_VERSION;

    md_tbl_t *ptbl = (md_tbl_t *)tbl;
    assert(md_reg_data_set(ptbl->start, (uint16_t*)&data, sizeof(dev_ro_data_t)) == 0);
    return 0;
}

/**
 * @brief 设备只读数据表注册
 * 
 */
static void __attribute__((constructor(103))) dev_ro_data_table_register(void)
{
    md_add_tbl(&(md_tbl_t){
        .start = DEV_RO_DATA_TBL_START,
        .end = DEV_RO_DATA_TBL_END,
        .is_write = TBL_READ_ONLY,
        .tbl_cb = dev_ro_data_tbl_handler,
    });
}

/**
 * @brief 设备读写数据表处理
 * 
 * @param tbl 数据表指针
 * @param cb_data 参数
 * @param priv_data 私有数据
 * @return 成功返回0，失败返回-1
 */
static int dev_rw_data_tbl_handler(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    dev_rw_data_t data = {0};
    cfg_param_t *cfg_param = parameter_get(CFG_PARAM_NAME);

    if (cb_data->is_write)
    {
        md_tbl_t *ptbl = (md_tbl_t *)tbl;
        assert(md_reg_data_get(ptbl->start, (uint16_t*)&data, sizeof(dev_rw_data_t)) == 0);

        int reg_nums = cb_data->reg_nums;
        uint16_t data_offset = (cb_data->reg_addr - ptbl->start) * 2;

        while (reg_nums > 0)
        {
            switch (data_offset)
            {
            case offsetof(dev_rw_data_t, wifi_sta_auth):
                data_offset += sizeof(data.wifi_sta_auth);
                reg_nums -= (sizeof(data.wifi_sta_auth) / 2);
                cfg_param->wifi_cfg.wifi_auth = data.wifi_sta_auth;
                break;
            
            case offsetof(dev_rw_data_t, wifi_sta_ssid):
                data_offset += sizeof(data.wifi_sta_ssid);
                reg_nums -= (sizeof(data.wifi_sta_ssid) / 2);
                memcpy(cfg_param->wifi_cfg.wifi_name, data.wifi_sta_ssid, sizeof(data.wifi_sta_ssid));
                break;

            case offsetof(dev_rw_data_t, wifi_sta_pwd):
                data_offset += sizeof(data.wifi_sta_pwd);
                reg_nums -= (sizeof(data.wifi_sta_pwd) / 2);
                memcpy(cfg_param->wifi_cfg.wifi_pwd, data.wifi_sta_pwd, sizeof(data.wifi_sta_pwd));
                break;

            case offsetof(dev_rw_data_t, cloud_addr):
                data_offset += sizeof(data.cloud_addr);
                reg_nums -= (sizeof(data.cloud_addr) / 2);
                memcpy(cfg_param->cloud_cfg.cloud_addr, data.cloud_addr, sizeof(data.cloud_addr));
                break;

            default:
                ESP_LOGW(TAG, "unknown modbus register: %d", (ptbl->start+data_offset)/2);
                break;
            }
        }
        
        assert(parameter_set(CFG_PARAM_NAME, cfg_param) == 0);
    }
    else
    {
        memcpy(&data.wifi_sta_auth, &cfg_param->wifi_cfg, 
               offsetof(dev_rw_data_t, reserved) - offsetof(dev_rw_data_t, wifi_sta_auth));
        memcpy(&data.cloud_addr, &cfg_param->cloud_cfg, sizeof(data.cloud_addr));

        md_tbl_t *ptbl = (md_tbl_t *)tbl;
        assert(md_reg_data_set(ptbl->start, (uint16_t*)&data, sizeof(dev_ro_data_t)) == 0);
    }
    return 0;
}

/**
 * @brief 设备读写数据表注册
 * 
 */
static void __attribute__((constructor(104))) dev_rw_data_table_register(void)
{
    md_add_tbl(&(md_tbl_t){
        .start = DEV_RW_DATA_TBL_START,
        .end = DEV_RW_DATA_TBL_END,
        .is_write = TBL_READ_WRITE,
        .tbl_cb = dev_rw_data_tbl_handler,
    });
}

/**
 * @brief mesh数据表处理
 * 
 * @param tbl 数据表指针
 * @param cb_data 参数
 * @param priv_data 私有数据
 * @return 成功返回0，失败返回-1
 */
static int mesh_data_tbl_handler(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    mesh_data_t data = {0};
    cfg_param_t *cfg_param = parameter_get(CFG_PARAM_NAME);

    if (cb_data->is_write)
    {
        md_tbl_t *ptbl = (md_tbl_t *)tbl;
        assert(md_reg_data_get(ptbl->start, (uint16_t*)&data, sizeof(mesh_data_t)) == 0);

        /* mesh id配置更新检查 */
        if (data.mesh_id1 != cfg_param->mesh_cfg.mesh_id.val) //...todo
        {
            cfg_param->mesh_cfg.mesh_id.val = data.mesh_id1;
        }

        assert(parameter_set(CFG_PARAM_NAME, cfg_param) == 0);
    }
    else
    {
        data.mesh_id1 = cfg_param->mesh_cfg.mesh_id.val;

        md_tbl_t *ptbl = (md_tbl_t *)tbl;
        assert(md_reg_data_set(ptbl->start, (uint16_t*)&data, sizeof(mesh_data_t)) == 0);
    }
    return 0;
}

/**
 * @brief mesh数据表注册
 * 
 */
static void __attribute__((constructor(105))) mesh_data_table_register(void)
{
    md_add_tbl(&(md_tbl_t){
        .start = MESH_DATA_TBL_START,
        .end = MESH_DATA_TBL_END,
        .is_write = TBL_READ_WRITE,
        .tbl_cb = mesh_data_tbl_handler,
    });
}

/**
 * @brief 插座只读数据表处理
 * 
 * @param tbl 数据表指针
 * @param cb_data 参数
 * @param priv_data 私有数据
 * @return 成功返回0，失败返回-1
 */
static int plug_ro_tbl_handler(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    if (cb_data->is_write) return -1;
    
    plug_ro_data_t data = {0};
    cfg_param_t *cfg_param = parameter_get(CFG_PARAM_NAME);
    memcpy(&data.dev_type, cfg_param->factory_cfg.dev_type, sizeof(data.dev_type));
    data.dev_sn = cfg_param->factory_cfg.dev_sn;
    data.soft_ver = SYS_FW_VERSION;

    energy_data_t *p_energy = energy_data_get();
    data.power = p_energy->power;
    data.voltage = p_energy->voltage;
    data.current = p_energy->current;
    data.freq = p_energy->freq;

    md_tbl_t *ptbl = (md_tbl_t *)tbl;
    assert(md_reg_data_set(ptbl->start, (uint16_t*)&data, sizeof(plug_ro_data_t)) == 0);
    return 0;
}

/**
 * @brief 插座只读数据表注册
 * 
 */
static void __attribute__((constructor(106))) plug_ro_table_register(void)
{
    md_add_tbl(&(md_tbl_t){
        .start = PLUG_RO_DATA_TBL_START,
        .end = PLUG_RO_DATA_TBL_END,
        .is_write = TBL_READ_ONLY,
        .tbl_cb = plug_ro_tbl_handler,
    });
}

/**
 * @brief 插座配置参数处理
 * 
 * @param new 新的配置参数
 * @param old 旧的配置参数
 * @return 成功返回0，失败返回-1 
 */
static int plug_config_handle(plug_rw_data_t *new, plug_rw_data_t *old)
{
    if (BIT_COMPARE(new->protect_en, old->protect_en)) {
        old->protect_en = new->protect_en;
    }

    if (BIT_COMPARE(new->setting1.mode, old->setting1.mode)) {
        old->setting1.mode = new->setting1.mode;
    }

    if (BIT_COMPARE(new->setting1.onoff, old->setting1.onoff)) {
        old->setting1.onoff = new->setting1.onoff;
    }

    if (BIT_COMPARE(new->setting1.over_load_en, old->setting1.over_load_en)) {
        old->setting1.over_load_en = new->setting1.over_load_en;
    }

    if (BIT_COMPARE(new->setting1.under_load_en, old->setting1.under_load_en)) {
        old->setting1.under_load_en = new->setting1.under_load_en;
    }

    if (BIT_COMPARE(new->setting1.delay_en, old->setting1.delay_en)) {
        old->setting1.delay_en = new->setting1.delay_en;
    }

    if (new->setting2.general_recover) {

    }

    if (new->setting2.advance_recover) {

    }

    if (new->setting2.stat_clear) {

    }

    if (BIT_COMPARE(new->timing_set1.timing_set1, old->timing_set1.timing_set1)) {
        old->timing_set1.timing_set1 = new->timing_set1.timing_set1;
    }

    if (BIT_COMPARE(new->timing_set1.timing_set2, old->timing_set1.timing_set2)) {
        old->timing_set1.timing_set2 = new->timing_set1.timing_set2;
    }

    if (BIT_COMPARE(new->timing_set1.timing_set3, old->timing_set1.timing_set3)) {
        old->timing_set1.timing_set3 = new->timing_set1.timing_set3;
    }

    if (BIT_COMPARE(new->timing_set1.timing_set4, old->timing_set1.timing_set4)) {
        old->timing_set1.timing_set4 = new->timing_set1.timing_set4;
    }

    if (BIT_COMPARE(new->timing_set1.timing_set5, old->timing_set1.timing_set5)) {
        old->timing_set1.timing_set5 = new->timing_set1.timing_set5;
    }

    if (BIT_COMPARE(new->timing_set1.timing_set6, old->timing_set1.timing_set6)) {
        old->timing_set1.timing_set6 = new->timing_set1.timing_set6;
    }

    if (BIT_COMPARE(new->timing_set1.timing_set7, old->timing_set1.timing_set7)) {
        old->timing_set1.timing_set7 = new->timing_set1.timing_set7;
    }

    if (BIT_COMPARE(new->timing_set1.timing_set8, old->timing_set1.timing_set8)) {
        old->timing_set1.timing_set8 = new->timing_set1.timing_set8;
    }

    if (BIT_COMPARE(new->timing_set2.timing_set9, old->timing_set2.timing_set9)) {
        old->timing_set2.timing_set9 = new->timing_set2.timing_set9;
    }

    if (BIT_COMPARE(new->timing_set2.timing_set10, old->timing_set2.timing_set10)) {
        old->timing_set2.timing_set10 = new->timing_set2.timing_set10;
    }

    if (new->over_load_limit != old->over_load_limit) {
        old->over_load_limit = new->over_load_limit;
    }

    if (new->under_load_limit != old->under_load_limit) {
        old->under_load_limit = new->under_load_limit;
    }

    if (new->led_brightness != old->led_brightness) {
        old->led_brightness = new->led_brightness;
    }

    if (new->timing_time != old->timing_time) {
        old->timing_time = new->timing_time;
    }

    for (int i = 0; i < sizeof(new->timing_args)/sizeof(new->timing_args[0]); i++) {
        if (new->timing_args[i].week_set.week != old->timing_args[i].week_set.week) {
            old->timing_args[i].week_set.week = new->timing_args[i].week_set.week;
        }

        if (BIT_COMPARE(new->timing_args[i].week_set.onoff, old->timing_args[i].week_set.onoff)) {
            old->timing_args[i].week_set.onoff = new->timing_args[i].week_set.onoff;
        }

        if (new->timing_args[i].time.all != old->timing_args[i].time.all) {
            old->timing_args[i].time.all = new->timing_args[i].time.all;
        }
    }

    return 0;
}

/**
 * @brief 插座读写数据表处理
 * 
 * @param tbl 数据表指针
 * @param cb_data 参数
 * @param priv_data 私有数据
 * @return 成功返回0，失败返回-1
 */
static int plug_rw_tbl_handler(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    plug_rw_data_t data = {0};
    plug_rw_data_t *plug_cfg = &(((plug_param_t*)(parameter_get(PLUG_PARAM_NAME)))->config);
    
    if (cb_data->is_write)
    {
        md_tbl_t *ptbl = (md_tbl_t *)tbl;
        assert(md_reg_data_get(ptbl->start, (uint16_t*)&data, sizeof(plug_rw_data_t)) == 0);

        if (plug_config_handle(&data, plug_cfg) != 0) {
            return -1;
        }

        assert(parameter_set(PLUG_PARAM_NAME, plug_cfg) == 0);
    }

    memcpy(&data, plug_cfg, sizeof(plug_rw_data_t));
    md_tbl_t *ptbl = (md_tbl_t *)tbl;
    assert(md_reg_data_set(ptbl->start, (uint16_t*)&data, sizeof(plug_rw_data_t)) == 0);
    return 0;
}

/**
 * @brief 插座读写数据表注册
 * 
 */
static void __attribute__((constructor(107))) plug_rw_table_register(void)
{
    md_add_tbl(&(md_tbl_t){
        .start = PLUG_RW_DATA_TBL_START,
        .end = PLUG_RW_DATA_TBL_END,
        .is_write = TBL_READ_WRITE,
        .tbl_cb = plug_rw_tbl_handler,
    });
}
#endif
