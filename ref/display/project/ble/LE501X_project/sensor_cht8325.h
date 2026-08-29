#ifndef __SENSOR_CHT8325_H__
#define __SENSOR_CHT8325_H__

// 读取湿度和温度单位为%RH和1°C，建议读取间隔1s以上
int i2c_read_env_ht(float *temperature, float *humidity);

void cht8325_check_init(void);
void cht8325_check_deinit(void);
void cht8325_check_start(void);
void cht8325_check_stop(void);

#endif // __SENSOR_CHT8325_H__
