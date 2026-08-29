/**
  ******************************************************************************
  * @file      off_grid_load_power.c
  * @version   1.0
  * @author    lixingyu
  * @date      2026/7/2
  * @brief     离网负载功率再分配
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/7/2   <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

/* ================================ 库文件引用 ================================ */
   
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
    
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
  
#include "esp_system.h"
#include "esp_log.h"
#include "sdkconfig.h"
  
/* ======================== 本地模块文件引用（可选） ============================= */

#include "off_grid_load_power.h"
#include "comm_define.h"

/* ================================ 文件内宏定义 ================================ */

#ifdef CONFIG_OFF_GRID_LOAD_POWER_DISTRIBUTION

#define TAG "[off_grid_load_power]"

/* =============================== 文件内全局变量 ================================ */

/*离网负载调度设备信息表*/
USE_EXT_RAM_BSS static off_grid_load_dev_t s_off_grid_dev_tbl[OFF_GRID_LOAD_DEV_MAX] = {0};

/* 离网负载功率互斥锁 */
static SemaphoreHandle_t xOffGridLoadPowerMutex = NULL;   

/* ================================ 模块函数定义 ================================ */

/* 简单封装：获取/释放互斥 */
static inline BaseType_t Off_Grid_Load_Power_TakeMutex(TickType_t timeout_ms)
{
    if (!xOffGridLoadPowerMutex) return pdFALSE;
    return xSemaphoreTake(xOffGridLoadPowerMutex, pdMS_TO_TICKS(timeout_ms));
}

static inline void Off_Grid_Load_Power_GiveMutex(void)
{
    if (xOffGridLoadPowerMutex) xSemaphoreGive(xOffGridLoadPowerMutex);
}

/**
 * @brief 离网负载功率模块初始化：创建互斥信号量
 * @return true 成功；false 创建失败
 */
bool off_grid_load_power_init(void)
{
    if (xOffGridLoadPowerMutex == NULL) {
        xOffGridLoadPowerMutex = xSemaphoreCreateMutex();
        if (xOffGridLoadPowerMutex == NULL) {
            ESP_LOGE(TAG, "off_grid_load_power_init: create mutex failed");
            return false;
        }
    }
    return true;
}

/* 按SN查找已存在条目，返回下标，未找到返回 -1 */
static int8_t off_grid_dev_find_by_sn(uint64_t sn)
{
    for (uint8_t i = 0; i < OFF_GRID_LOAD_DEV_MAX; i++) {
        if (s_off_grid_dev_tbl[i].dev_sn == sn) return (int8_t)i;
    }
    return -1;
}

/* 查找空槽位（dev_sn==0），未找到返回 -1 */
static int8_t off_grid_dev_find_empty(void)
{
    for (uint8_t i = 0; i < OFF_GRID_LOAD_DEV_MAX; i++) {
        if (s_off_grid_dev_tbl[i].dev_sn == 0) return (int8_t)i;
    }
    return -1;
}

/**
 * @brief 记录/更新一条离网负载设备信息（按 dev_sn 匹配：存在则更新，不存在则写入空槽）
 * @note  更新已存在条目时，exp_off_grid_power（期望输出离网负载功率）由调度算法维护，
 *        不被本函数覆盖，保留原值；其余字段按 dev 更新。新增条目时按 dev 整体写入。
 * @param dev 设备信息指针（dev_sn 不能为 0）
 * @return true 成功；false 参数非法/取锁失败/表满
 */
bool off_grid_load_dev_upsert(const off_grid_load_dev_t *dev)
{
    bool taken = false;
    bool ok = false;

    if (dev == NULL || dev->dev_sn == 0) {
        ESP_LOGE(TAG, "upsert: invalid dev or sn=0");
        return false;
    }
    if (!Off_Grid_Load_Power_TakeMutex(100)) {
        ESP_LOGW(TAG, "upsert: take mutex failed");
        return false;
    }
    taken = true;

    int8_t idx = off_grid_dev_find_by_sn(dev->dev_sn);
    if (idx >= 0) {                                  /* 更新：保留 exp_off_grid_power */
        int16_t keep_exp = s_off_grid_dev_tbl[idx].exp_off_grid_power;
        s_off_grid_dev_tbl[idx] = *dev;
        s_off_grid_dev_tbl[idx].exp_off_grid_power = keep_exp;
        ok = true;
        ESP_LOGD(TAG, "upsert update [%d] sn=0x%llx", idx, dev->dev_sn);
    } else {
        idx = off_grid_dev_find_empty();            /* 新增：整体写入 */
        if (idx < 0) {
            ESP_LOGW(TAG, "upsert: table full");
        } else {
            s_off_grid_dev_tbl[idx] = *dev;
            ok = true;
            ESP_LOGD(TAG, "upsert add [%d] sn=0x%llx", idx, dev->dev_sn);
        }
    }

    if (taken) Off_Grid_Load_Power_GiveMutex();
    return ok;
}

/**
 * @brief 从设备表删除指定设备（按 dev_sn 匹配）
 * @param sn 设备SN
 * @return true 找到并删除；false 未找到/取锁失败
 */
bool off_grid_load_dev_delete(uint64_t sn)
{
    bool taken = false;

    if (sn == 0) return false;
    if (!Off_Grid_Load_Power_TakeMutex(100)) {
        ESP_LOGW(TAG, "delete: take mutex failed");
        return false;
    }
    taken = true;

    int8_t idx = off_grid_dev_find_by_sn(sn);
    bool found = (idx >= 0);
    if (found) {
        memset(&s_off_grid_dev_tbl[idx], 0, sizeof(off_grid_load_dev_t));
        ESP_LOGD(TAG, "delete [%d] sn=0x%llx", idx, sn);
    }

    if (taken) Off_Grid_Load_Power_GiveMutex();
    return found;
}

/**
 * @brief 离网负载功率主调度
 * @note  优先级：Σexp == sum_req（总量守恒） > soc 比例分配；[MIN,MAX] 为硬边界。
 *        步骤：
 *        1) 有效设备 = sched_allowed && dev_sn != 0；
 *           sum_req = Σ req_off_grid_power，sum_soc = Σ dev_soc；
 *        2) 按 soc 比例初分配 exp[i] = sum_req*dev_soc[i]/sum_soc
 *           （sum_soc==0 退化为均分），并先钳位到 [MIN,MAX]；
 *        3) 计算 deficit = sum_req - Σexp（钳位/取整导致偏差）；
 *           deficit>0：把缺口分给仍有上限余量(exp<MAX)的设备；
 *           deficit<0：把盈余从仍有下限余裕(exp>MIN)的设备收回；
 *           循环直至 deficit==0 或无可用余量（此时总需求超出[ΣMIN,ΣMAX]容量，
 *           属不可守恒边界情形，无法继续）。
 *        非参与调度设备 exp_off_grid_power 清 0。
 * @return 参与调度的有效设备数（0 表示无有效设备或取锁失败）
 */
uint8_t off_grid_load_power_schedule(void)
{
    if (!Off_Grid_Load_Power_TakeMutex(200)) {
        ESP_LOGW(TAG, "schedule: take mutex failed");
        return 0;
    }

    /* 第一遍：筛选有效设备，统计总soc/总req；非参与设备清零期望输出 */
    uint8_t  valid_cnt = 0;
    uint8_t  idx_map[OFF_GRID_LOAD_DEV_MAX];
    uint32_t sum_soc = 0;
    uint32_t sum_req = 0;

    for (uint8_t i = 0; i < OFF_GRID_LOAD_DEV_MAX; i++) {
        off_grid_load_dev_t *d = &s_off_grid_dev_tbl[i];
        if (d->dev_sn != 0 && d->sched_allowed) {
            idx_map[valid_cnt++] = i;
            sum_soc += d->dev_soc;
            sum_req += d->req_off_grid_power;
        } else {
            d->exp_off_grid_power = 0;
        }
    }
    if (valid_cnt == 0) {
        Off_Grid_Load_Power_GiveMutex();
        return 0;
    }

    /* 第二遍：按 soc 比例初分配（sum_soc==0 退化为均分），并钳位到 [MIN,MAX] */
    int32_t assigned = 0;
    for (uint8_t k = 0; k < valid_cnt; k++) {
        off_grid_load_dev_t *d = &s_off_grid_dev_tbl[idx_map[k]];
        int32_t share = (sum_soc > 0)
            ? (((int32_t)sum_req * (int32_t)d->dev_soc) / (int32_t)sum_soc)
            : ((int32_t)sum_req / valid_cnt);
        if (share > OFF_GRID_DEV_MAX_POWER) share = OFF_GRID_DEV_MAX_POWER;
        else if (share < OFF_GRID_DEV_MIN_POWER) share = OFF_GRID_DEV_MIN_POWER;
        d->exp_off_grid_power = (int16_t)share;
        assigned += d->exp_off_grid_power;
    }

    /* 第三遍：修正 Σexp 与 sum_req 的偏差，优先满足总量守恒（在 [MIN,MAX] 内） */
    int32_t deficit = (int32_t)sum_req - (int32_t)assigned;
    while (deficit != 0) {
        bool progressed = false;
        if (deficit > 0) {                              /* 缺口：分给 exp<MAX 的设备 */
            uint8_t cnt = 0;
            for (uint8_t k = 0; k < valid_cnt; k++)
                if (s_off_grid_dev_tbl[idx_map[k]].exp_off_grid_power < OFF_GRID_DEV_MAX_POWER) cnt++;
            if (cnt == 0) break;
            int32_t step = deficit / cnt; if (step == 0) step = 1;
            for (uint8_t k = 0; k < valid_cnt && deficit > 0; k++) {
                int16_t *p = &s_off_grid_dev_tbl[idx_map[k]].exp_off_grid_power;
                if (*p >= OFF_GRID_DEV_MAX_POWER) continue;
                int32_t head = (int32_t)OFF_GRID_DEV_MAX_POWER - *p;
                int32_t add = head < step ? head : step;
                if (add > deficit) add = deficit;
                *p += (int16_t)add; deficit -= add; progressed = true;
            }
        } else {                                        /* 盈余：从 exp>MIN 的设备收回 */
            uint8_t cnt = 0;
            for (uint8_t k = 0; k < valid_cnt; k++)
                if (s_off_grid_dev_tbl[idx_map[k]].exp_off_grid_power > OFF_GRID_DEV_MIN_POWER) cnt++;
            if (cnt == 0) break;
            int32_t step = (-deficit) / cnt; if (step == 0) step = 1;
            for (uint8_t k = 0; k < valid_cnt && deficit < 0; k++) {
                int16_t *p = &s_off_grid_dev_tbl[idx_map[k]].exp_off_grid_power;
                if (*p <= OFF_GRID_DEV_MIN_POWER) continue;
                int32_t slack = (int32_t)*p - OFF_GRID_DEV_MIN_POWER;
                int32_t sub = slack < step ? slack : step;
                if (sub > -deficit) sub = -deficit;
                *p -= (int16_t)sub; deficit += sub; progressed = true;
            }
        }
        if (!progressed) break;
    }

    ESP_LOGD(TAG, "schedule: valid=%u sum_soc=%lu sum_req=%lu final_deficit=%ld",
             valid_cnt, (unsigned long)sum_soc, (unsigned long)sum_req, (long)deficit);

    Off_Grid_Load_Power_GiveMutex();
    return valid_cnt;
}

/**
 * @brief 获取指定SN设备的期望输出离网负载功率
 * @param sn 设备SN
 * @param exp_out 找到时写入 exp_off_grid_power
 * @return true 找到；false 未找到/参数非法/取锁失败
 */
bool off_grid_load_dev_get_exp(uint64_t sn, int16_t *exp_out)
{
    bool taken = false;
    bool found = false;

    if (sn == 0 || exp_out == NULL) return false;
    if (!Off_Grid_Load_Power_TakeMutex(100)) {
        ESP_LOGW(TAG, "get_exp: take mutex failed");
        return false;
    }
    taken = true;

    int8_t idx = off_grid_dev_find_by_sn(sn);
    if (idx >= 0) {
        *exp_out = s_off_grid_dev_tbl[idx].exp_off_grid_power;
        found = true;
    }

    if (taken) Off_Grid_Load_Power_GiveMutex();
    return found;
}


/* ============================== 调试自测（可选） =============================== */
#ifdef OFF_GRID_LOAD_POWER_TEST

#include <inttypes.h>

/*
I (723554) [oglp_test]: ==== off_grid_load_power self test start ====
I (723554) [oglp_test]:   [PASS] case1 empty schedule
I (723556) [oglp_test]:   [PASS] case2 single sum==req
I (723561) [oglp_test]:   [PASS] case3 proportional sum==req
I (723568) [oglp_test]:   [PASS] case4 soc0 equal split sum==req
I (723574) [oglp_test]:   [PASS] case5 capacity edge sum==req
I (723581) [oglp_test]:   [PASS] case6 infeasible capped at MAX
I (723587) [oglp_test]:   [PASS] case7 disallowed exp=0
I (723593) [oglp_test]:   [PASS] case8 negative req clamped to MIN
I (723600) [oglp_test]:   [PASS] case9 upsert preserves exp
I (723606) [oglp_test]:   [PASS] case10 delete existing
I (723612) [oglp_test]:   [PASS] case10b delete missing
I (723618) [oglp_test]:   [PASS] case10c sn gone after delete
I (723630) [oglp_test]:   [PASS] case11 table full reject
I (723643) [oglp_test]:   [PASS] case12 null reject
I (723655) [oglp_test]:   [PASS] case12b sn0 reject
I (723660) [oglp_test]:   [PASS] case12c delete0 reject
I (723666) [oglp_test]:   [PASS] case13 all exp in [MIN,MAX]
I (723673) [oglp_test]: ==== self test done: pass=17 fail=0 ====
*/

/* 求所有设备 exp_off_grid_power 之和 */
static int32_t test_sum_exp(void)
{
    int32_t s = 0;
    for (uint8_t i = 0; i < OFF_GRID_LOAD_DEV_MAX; i++)
        s += s_off_grid_dev_tbl[i].exp_off_grid_power;
    return s;
}

/* 清空设备表（持锁） */
static void test_reset_table(void)
{
    if (Off_Grid_Load_Power_TakeMutex(100)) {
        memset(s_off_grid_dev_tbl, 0, sizeof(s_off_grid_dev_tbl));
        Off_Grid_Load_Power_GiveMutex();
    }
}

#define TEST_CHECK(cond, name, ...) do {                                    \
        if (cond) { pass++; ESP_LOGI(t, "  [PASS] %s", name); }             \
        else      { fail++; ESP_LOGE(t, "  [FAIL] %s " __VA_ARGS__, name); } \
    } while (0)

/**
 * @brief 离网负载功率模块非阻塞自测，覆盖各类边界与守恒场景
 * @return 失败用例数（0=全通过；0xFF=初始化失败）
 */
uint8_t off_grid_load_power_self_test(void)
{
    static const char *t = "[oglp_test]";
    uint8_t fail = 0, pass = 0;
    off_grid_load_dev_t d;

    ESP_LOGI(t, "==== off_grid_load_power self test start ====");
    if (!off_grid_load_power_init()) { ESP_LOGE(t, "init failed, abort"); return 0xFF; }

    /* 1) 空表调度 */
    test_reset_table();
    TEST_CHECK(off_grid_load_power_schedule() == 0, "case1 empty schedule");

    /* 2) 单设备 req<MAX → Σexp==req */
    test_reset_table();
    d = (off_grid_load_dev_t){.dev_type=1,.dev_sn=1001,.dev_soc=80,
                              .req_off_grid_power=300,.sched_allowed=true};
    off_grid_load_dev_upsert(&d);
    off_grid_load_power_schedule();
    TEST_CHECK(test_sum_exp() == 300, "case2 single sum==req", "(sum=%ld)", (long)test_sum_exp());

    /* 3) 多设备 soc 比例分配 + 守恒（sum_req=500,sum_soc=100）*/
    test_reset_table();
    d=(off_grid_load_dev_t){.dev_type=1,.dev_sn=1,.dev_soc=30,.req_off_grid_power=200,.sched_allowed=true}; off_grid_load_dev_upsert(&d);
    d=(off_grid_load_dev_t){.dev_type=1,.dev_sn=2,.dev_soc=70,.req_off_grid_power=300,.sched_allowed=true}; off_grid_load_dev_upsert(&d);
    off_grid_load_power_schedule();
    TEST_CHECK(test_sum_exp() == 500, "case3 proportional sum==req", "(sum=%ld)", (long)test_sum_exp());

    /* 4) 全部 soc=0 → 均分 + 守恒 */
    test_reset_table();
    d=(off_grid_load_dev_t){.dev_type=1,.dev_sn=1,.dev_soc=0,.req_off_grid_power=100,.sched_allowed=true}; off_grid_load_dev_upsert(&d);
    d=(off_grid_load_dev_t){.dev_type=1,.dev_sn=2,.dev_soc=0,.req_off_grid_power=100,.sched_allowed=true}; off_grid_load_dev_upsert(&d);
    off_grid_load_power_schedule();
    TEST_CHECK(test_sum_exp() == 200, "case4 soc0 equal split sum==req", "(sum=%ld)", (long)test_sum_exp());

    /* 5) 恰好满容量：3 设备各 req=MAX(1000) → Σexp==3000 */
    test_reset_table();
    for (uint8_t i=1;i<=3;i++){ d=(off_grid_load_dev_t){.dev_type=1,.dev_sn=i,.dev_soc=50,.req_off_grid_power=1000,.sched_allowed=true}; off_grid_load_dev_upsert(&d);}
    off_grid_load_power_schedule();
    TEST_CHECK(test_sum_exp() == 3000, "case5 capacity edge sum==req", "(sum=%ld)", (long)test_sum_exp());

    /* 6) 不可守恒：3 设备各 req=2000，sum_req=6000 > ΣMAX=3000 → 各顶 MAX */
    test_reset_table();
    for (uint8_t i=1;i<=3;i++){ d=(off_grid_load_dev_t){.dev_type=1,.dev_sn=i,.dev_soc=50,.req_off_grid_power=2000,.sched_allowed=true}; off_grid_load_dev_upsert(&d);}
    off_grid_load_power_schedule();
    {   int32_t s=test_sum_exp(); bool all_max=true;
        for (uint8_t i=0;i<3;i++) if (s_off_grid_dev_tbl[i].exp_off_grid_power!=OFF_GRID_DEV_MAX_POWER) all_max=false;
        TEST_CHECK(s==3000 && all_max, "case6 infeasible capped at MAX", "(sum=%ld)", (long)s);
    }

    /* 7) sched_allowed=false 设备不参与，exp=0 */
    test_reset_table();
    d=(off_grid_load_dev_t){.dev_type=1,.dev_sn=1,.dev_soc=50,.req_off_grid_power=200,.sched_allowed=true}; off_grid_load_dev_upsert(&d);
    d=(off_grid_load_dev_t){.dev_type=1,.dev_sn=2,.dev_soc=50,.req_off_grid_power=300,.sched_allowed=false}; off_grid_load_dev_upsert(&d);
    off_grid_load_power_schedule();
    {   int8_t i1=off_grid_dev_find_by_sn(1), i2=off_grid_dev_find_by_sn(2);
        TEST_CHECK(i1>=0 && s_off_grid_dev_tbl[i1].exp_off_grid_power==200 &&
                   i2>=0 && s_off_grid_dev_tbl[i2].exp_off_grid_power==0,
                   "case7 disallowed exp=0");
    }

    /* 8) 负功率：req 为负 → 钳位到 MIN=0，Σexp==0 */
    test_reset_table();
    d=(off_grid_load_dev_t){.dev_type=1,.dev_sn=1,.dev_soc=50,.req_off_grid_power=-400,.sched_allowed=true}; off_grid_load_dev_upsert(&d);
    off_grid_load_power_schedule();
    TEST_CHECK(test_sum_exp() == 0, "case8 negative req clamped to MIN", "(sum=%ld)", (long)test_sum_exp());

    /* 9) upsert 更新保留 exp_off_grid_power */
    test_reset_table();
    d=(off_grid_load_dev_t){.dev_type=1,.dev_sn=1,.dev_soc=50,.req_off_grid_power=200,.sched_allowed=true}; off_grid_load_dev_upsert(&d);
    off_grid_load_power_schedule();
    {   int8_t i=off_grid_dev_find_by_sn(1);
        int16_t before=(i>=0)?s_off_grid_dev_tbl[i].exp_off_grid_power:0;
        d=(off_grid_load_dev_t){.dev_type=1,.dev_sn=1,.dev_soc=90,.req_off_grid_power=500,.exp_off_grid_power=999,.sched_allowed=true};
        off_grid_load_dev_upsert(&d);
        i=off_grid_dev_find_by_sn(1);
        int16_t after=(i>=0)?s_off_grid_dev_tbl[i].exp_off_grid_power:0;
        TEST_CHECK(after==before, "case9 upsert preserves exp", "(b=%d a=%d)", before, after);
    }

    /* 10) delete 生效 / 删除不存在 / sn 消失 */
    test_reset_table();
    d=(off_grid_load_dev_t){.dev_type=1,.dev_sn=7,.dev_soc=50,.req_off_grid_power=200,.sched_allowed=true}; off_grid_load_dev_upsert(&d);
    TEST_CHECK(off_grid_load_dev_delete(7)==true,  "case10 delete existing");
    TEST_CHECK(off_grid_load_dev_delete(7)==false, "case10b delete missing");
    TEST_CHECK(off_grid_dev_find_by_sn(7)<0,       "case10c sn gone after delete");

    /* 11) 表满拒绝 */
    test_reset_table();
    for (uint8_t i=1;i<=OFF_GRID_LOAD_DEV_MAX;i++){ d=(off_grid_load_dev_t){.dev_type=1,.dev_sn=i,.dev_soc=50,.req_off_grid_power=100,.sched_allowed=true}; off_grid_load_dev_upsert(&d);}
    d=(off_grid_load_dev_t){.dev_type=1,.dev_sn=999,.dev_soc=50,.req_off_grid_power=100,.sched_allowed=true};
    TEST_CHECK(off_grid_load_dev_upsert(&d)==false, "case11 table full reject");

    /* 12) 非法参数 */
    TEST_CHECK(off_grid_load_dev_upsert(NULL)==false, "case12 null reject");
    d=(off_grid_load_dev_t){.dev_sn=0};
    TEST_CHECK(off_grid_load_dev_upsert(&d)==false,  "case12b sn0 reject");
    TEST_CHECK(off_grid_load_dev_delete(0)==false,   "case12c delete0 reject");

    /* 13) 综合边界：所有有效设备 exp ∈ [MIN,MAX] */
    test_reset_table();
    for (uint8_t i=1;i<=5;i++){ d=(off_grid_load_dev_t){.dev_type=1,.dev_sn=i,.dev_soc=i*15,.req_off_grid_power=i*250,.sched_allowed=true}; off_grid_load_dev_upsert(&d);}
    off_grid_load_power_schedule();
    {   bool in_range=true;
        for (uint8_t i=0;i<OFF_GRID_LOAD_DEV_MAX;i++){
            if (s_off_grid_dev_tbl[i].dev_sn!=0){
                int16_t e=s_off_grid_dev_tbl[i].exp_off_grid_power;
                if (e<OFF_GRID_DEV_MIN_POWER || e>OFF_GRID_DEV_MAX_POWER) in_range=false;
            }
        }
        TEST_CHECK(in_range, "case13 all exp in [MIN,MAX]");
    }

    test_reset_table();
    ESP_LOGI(t, "==== self test done: pass=%u fail=%u ====", pass, fail);
    return fail;
}


#endif /* OFF_GRID_LOAD_POWER_TEST */

#endif

