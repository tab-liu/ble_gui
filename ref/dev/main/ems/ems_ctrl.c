#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "ems_ctrl.h"

#include "uart_device_process.h"

#include "iot_period_task.h"
#include "ll_param_def.h"
#include "can_protocol.h"
#include "can_control.h"
#include "can_init.h"
#include "can_data.h"



#define TAG     "[ems_ctrl]"

EXT_RAM_BSS_ATTR struct PARLLEL_EMS_PRIVATE  EmsPrivate = {0};
EXT_RAM_BSS_ATTR struct PARLLEL_EMS_PUBLIC   EmsPublic[MAXPARLLELNUM]= {0};
EXT_RAM_BSS_ATTR struct PARLLEL_SET          EmsSetParameter= {0};

EXT_RAM_BSS_ATTR struct PARLLEL_SETTING      EmsSetData[MAXPARLLELNUM]= {0};
EXT_RAM_BSS_ATTR EMS_PID_STRUCT ems_pid[MAXINVPHASE];//3 phase






/*New------------------------------------------------------------------------*/
/*------------------------------------------------------------------------
*@Function：EMSSettingParamterInput
-------------------------------------------------------------------------*/
/**
*@brief EMS Public参数输入
*@param[in]     Index：设备序号
*@param[out]    None
*@return        1-执行成功，0-执行失败
*/
uint8_t EMSPublicDataInput(uint16_t index0)
{
    if(index0 >= MAXPARLLELNUM)
    {
        return 0;
    }
    struct PARLLEL_EMS_PUBLIC *p_EmsPublic = &EmsPublic[index0];//0 2
    uint16_t index = EmsPrivate.INVSerialIndex[index0];//A104 真实的硬件索引号;假设INV2离线，EmsPublic[1]的数据来源是INV3
    if(index >= MAXPARLLELNUM)
    {
        return 0;
    }
	
	const inv_pv_struct *inv_pv = &Inv_can[index].inv_data[0].inv_pv;	//inv_set00_struct *inv_set00 = &g_device_data.inv_data[index][0].inv_set00;
    const inv_base_param_t *inv_set00 = &SetData_Can.dev_info_t2.inv_set00;
	const inv_base_struct *inv_base0 = &Inv_can[index].inv_data[0].inv_base;
    const inv_grid_struct *inv_grid = &Inv_can[index].inv_data[0].inv_grid;
    const inv_data_struct   *inv_data = &Inv_can[index].inv_data[0].inv_data;
    const pack_announce_struct *pack_announce = &Inv_can[index].pack_data[0].pack_announce;
    const inv_about_struct  *inv_about = &Inv_can[index].inv_data[0].inv_about;
    const inv_announce_struct *inv_announce = &Inv_can[index].inv_data[0].inv_announce;

//    ac_couple_meter_data_t *ac_power_meter = ac_couple_meter_data_get();
//    grid_meter_data_t *grid_power_meter = grid_meter_data_get();


    EmsPrivate.SingleCapacityPower[index0] = pack_announce->pack_cnt*Inv_can[index].pack_data[0].pack_extend.capacity;//A130.1
    
    //PARLLEL_EMS_PUBLIC
    //InvLimit
    //A123 触发CT监测的时候，要做限制，预防电网过载
//    if(g_device_data.bk_inv_dev_set.inv_set01.ct_test.ct_enable)
//    {
//        ESP_LOGW(TAG,"Warning CT Testing ... make Power limit to 500W,ct_enbale:%d",
//                g_device_data.bk_inv_dev_set.inv_set01.ct_test.ct_enable);
//        p_EmsPublic->InvLimit.PhaseMaxChgPower[0] = 500;//每相最大充电功率限值 4500
//        p_EmsPublic->InvLimit.PhaseMaxChgPower[1] = 500;//每相最大充电功率限值 4500
//        p_EmsPublic->InvLimit.PhaseMaxChgPower[2] = 500;//每相最大充电功率限值
//
//        p_EmsPublic->InvLimit.PhaseMaxDisChgPower[0] = 500;//最大放电功率限值 4500
//        p_EmsPublic->InvLimit.PhaseMaxDisChgPower[1] = 500;//最大放电功率限值
//        p_EmsPublic->InvLimit.PhaseMaxDisChgPower[2] = 500;//最大放电功率限值
//    }
//    else
    {
        p_EmsPublic->InvLimit.PhaseMaxChgPower[0] = inv_announce->l1_chg_limit;//每相最大充电功率限值 4500
        p_EmsPublic->InvLimit.PhaseMaxChgPower[1] = inv_announce->l2_chg_limit;//每相最大充电功率限值 4500
        p_EmsPublic->InvLimit.PhaseMaxChgPower[2] = inv_announce->l3_chg_limit;//每相最大充电功率限值

        p_EmsPublic->InvLimit.PhaseMaxDisChgPower[0] = inv_announce->l1_dsg_limit;//最大放电功率限值 4500
        p_EmsPublic->InvLimit.PhaseMaxDisChgPower[1] = inv_announce->l2_dsg_limit;//最大放电功率限值
        p_EmsPublic->InvLimit.PhaseMaxDisChgPower[2] = inv_announce->l3_dsg_limit;//最大放电功率限值
    }
    // if(EmsPublic[0].InvDetail.InvType == EP760InvType && EmsSetParameter.SetCountry == 0)//0:在德国 EmsSetParameter.SetCountry == 0)
    // {
    //     p_EmsPublic->InvLimit.PhaseMaxDisChgPower[0] = 4600U;//最大放电功率限值
    //     p_EmsPublic->InvLimit.PhaseMaxDisChgPower[1] = 4600U;//最大放电功率限值
    //     p_EmsPublic->InvLimit.PhaseMaxDisChgPower[2] = 4600U;//最大放电功率限值
    // }
    if(!p_EmsPublic->InvLimit.PhaseMaxChgPower[0])//用于检测逆变器并机使能失败的工况
    {
        ESP_LOGW(TAG,"Error:inv_announce->l1_chg_limit:%d,%d",inv_announce->l1_chg_limit,inv_announce->l2_chg_limit);
    }
    p_EmsPublic->InvDetail.InvSoc = pack_announce->soc;//电池电量
    p_EmsPublic->InvDetail.SocThousand = pack_announce->soc2;//0-1000 0.1%,与soc是10倍关系
    p_EmsPublic->InvDetail.PackRunState = pack_announce->work_status;
    p_EmsPublic->InvDetail.PackMaxChgCurrent = pack_announce->max_chg_current;
    p_EmsPublic->InvDetail.PackMaxDisCurrent = pack_announce->max_dsg_current;
    p_EmsPublic->InvDetail.PackTotalVoltage = pack_announce->total_voltage;

#if 0
    p_EmsPublic->InvDetail.PackTotalCurrent = pack_announce->total_current;
#else
    p_EmsPublic->InvDetail.PackTotalCurrent = 30000 - pack_announce->TotalCurrent_bias;//M107 pack_announce->total_current;//A106
    p_EmsPublic->InvDetail.PackTotalMaxChgCurrent = p_EmsPublic->InvDetail.PackTotalCurrent;//A110
#endif
    p_EmsPublic->InvDetail.TotalMainPackCnts = pack_announce->pack_cnt;
    p_EmsPublic->InvDetail.LowTempChgFlag = pack_announce->status1.bit.battery_heat;//BMS控制状态1 ，启动加热
    //p_EmsPublic->InvDetail.BmsUpGradeFlag = pack_announce->;
    p_EmsPublic->InvDetail.EmergencyChgFlag = pack_announce->status1.bit.chg_now;//! chg_now
    // EmsPrivate.BMSChargeProtect[index0] = pack_announce->status1.bit.chg_protect;
//    if(strncmp(inv_about->dev_type,"EP600",5)==0)//机型
//    {
//        p_EmsPublic->InvDetail.InvType = 1;
//    }
//    else if(strncmp(inv_about->dev_type,"EP900",5)==0)
//    {
//        p_EmsPublic->InvDetail.InvType = 2;
//    }
//    else if(strncmp(inv_about->dev_type,"EP800",5)==0)
//    {
//        p_EmsPublic->InvDetail.InvType = 3;
//    }
//    else if(strncmp(inv_about->dev_type,"EP760",5)==0)
//    {
//        p_EmsPublic->InvDetail.InvType = 4;
//    }
//    else if(strncmp(inv_about->dev_type,"EP2000",6)==0)
//    {
//        p_EmsPublic->InvDetail.InvType = 5;
//    }
//    else
//    {
//        ESP_LOGE(TAG,"Error:EMSPublicDataInput*[%d]:dev_type:%s",index,inv_about->dev_type);
//        p_EmsPublic->InvDetail.InvType = 0;
//    }
    p_EmsPublic->InvDetail.MachineSN = inv_about->dev_sn;
    p_EmsPublic->InvDetail.EMSWorkMode = inv_set00->work_mode;//工作模式暂定义 PV优先 UPS //????
     
    p_EmsPublic->InvDetail.EMSFaultCode |= (inv_announce->overall_status) & 0x7F;//后续改用位  底层发给控制器的 可包含所有相关故障位 //bit0~bit6
    p_EmsPublic->InvDetail.ParallelInvState = inv_base0->inv_work_state;//逆变器工作状态  *0：停机；1：离网运行；2：电网带载；3：并网运行；4：并网充电；5：并网放电；
    //p_EmsPublic->InvDetail.DC_PVPower = inv_base0->PVAllTotalPower;
    //p_EmsPublic->InvDetail.DC_PVPower = inv_base0->PVAllTotalPower * EP900_PV_DC_EFF / 100;//D107

    //针对EP800加入ATS后，电网电压略微偏离并网运行时正常电压的处理 A139.4.3
//    if((EmsPublic[0].InvDetail.InvType == EP800InvType) && (EmsSetParameter.ATS_Enable == 1))
//    {
//        if((abs(grid_power_meter->WphA) > SinglePhasePowerGrid_150W) && (p_EmsPublic->InvDetail.ParallelInvState == 1))
//        {
//            p_EmsPublic->InvDetail.ParallelInvState = 2;
//            ESP_LOGE(TAG,"ParallelInvState:%d",p_EmsPublic->InvDetail.ParallelInvState);
//        }
//    }

//    InvTypeJudge();

    //Dc_Pv_Power A107
    uint16_t dc_pv_num = 0U;
    uint16_t Dc_Pv_Power = 0U;
    uint16_t i = 0U;
//    for(i = 0U; i < MaxPv5TypeCount; i++)
//    {
//        if(inv_pv->pv_detail[i].input_type == 100)//Dc_Pv_Power
//        {
//            if(dc_pv_num < inv_pv->pv_number.dc_pv_numbers)
//            {
//                Dc_Pv_Power += inv_pv->pv_detail[i].input_power;
//                p_EmsPublic->InvDetail.DC_PVVoltage[dc_pv_num] = inv_pv->pv_detail[i].input_voltage;
//            }
//            else
//            {
//                break;
//            }
//            dc_pv_num++;
//        }
//    }
    p_EmsPublic->InvDetail.DC_PVPower = Dc_Pv_Power; //* EP900_PV_DC_EFF / 100U; D132
    // ESP_LOGI(TAG,"number:%d,dc_pv_num:%d,Dc_Pv_Power:%d,DC_PVPower:%d\n",inv_pv->pv_number.dc_pv_numbers,dc_pv_num,
    //          Dc_Pv_Power,p_EmsPublic->InvDetail.DC_PVPower);

//    if((grid_power_meter->Frequency > 4000) && (grid_power_meter->Frequency < 7000))//M131
//    {
//        p_EmsPublic->InvDetail.GridFre = grid_power_meter->Frequency;//inv_grid->freq;//电网频率
//        p_EmsPublic->InvDetail.GridVoltage = grid_power_meter->PhaseVoltageAN;//120V
//        //A138.8
//        EmsPrivate.GridPower[0] = grid_power_meter->WphA;
//        EmsPrivate.GridPower[1] = grid_power_meter->WphB;
//        EmsPrivate.GridPower[2] = grid_power_meter->WphC;
//    }
//    else
//    {
//        ESP_LOGE(TAG, "Error:GridMeter:ID = %d", grid_power_meter->ID);
//        //D138.9
//        // p_EmsPublic->InvDetail.GridFre = inv_grid->freq;//inv_grid->freq;//电网频率
//        // p_EmsPublic->InvDetail.GridVoltage = inv_grid->grid_detail[0].input_voltage;//电网电压
//
//        if(EmsPublic[0].InvDetail.InvType == EP800InvType)
//        {
//            // p_EmsPublic->InvDetail.GridVoltage = g_device_data.inv_summary.inv_load.ac_load[index].load_voltage;
//            p_EmsPublic->InvDetail.GridVoltage = UploadEMSData.SysEnergyData.grid_detail[0].input_voltage;//M139 M139.2 0 index是台数 这里应该是相数,直接取R相电网电压！！！
//        }
//        else
//        {
//            p_EmsPublic->InvDetail.GridFre =0;
//            p_EmsPublic->InvDetail.GridVoltage =0;
//            EmsPrivate.GridPower[0] = 0;
//            EmsPrivate.GridPower[1] = 0;
//            EmsPrivate.GridPower[2] = 0;
//        }
//    }

    p_EmsPublic->InvDetail.InvPower[0] = inv_data->inv_detail[0].power;
    p_EmsPublic->InvDetail.InvPower[1] = inv_data->inv_detail[1].power;
    p_EmsPublic->InvDetail.InvPower[2] = inv_data->inv_detail[2].power;

    p_EmsPublic->InvDetail.ELoadApparent[0] = inv_announce->l1_apparent_power;
    p_EmsPublic->InvDetail.ELoadApparent[1] = inv_announce->l2_apparent_power;
    p_EmsPublic->InvDetail.ELoadApparent[2] = inv_announce->l3_apparent_power;

    // if(ac_power_meter->Frequency != 0)
//    if((ac_power_meter->Frequency > 4000) && (ac_power_meter->Frequency < 7000))//M131 
//    {
//
//            //需要判断是否小于零,需要判断，欧标和美标，协议不一致
//            p_EmsPublic->InvDetail.AC_PvPower[0] = (ac_power_meter->WphA > 0) ? ac_power_meter->WphA : 0;//微逆电表
//            p_EmsPublic->InvDetail.AC_PvPower[1] = (ac_power_meter->WphB > 0) ? ac_power_meter->WphB : 0;//微逆电表
//            p_EmsPublic->InvDetail.AC_PvPower[2] = (ac_power_meter->WphC > 0) ? ac_power_meter->WphC : 0;//ac_power_meter->WphC;//微逆电表
//
//            EmsPrivate.AC_PVPower[0] = (ac_power_meter->WphA > 0) ? ac_power_meter->WphA : 0;//微逆电表
//            EmsPrivate.AC_PVPower[1] = (ac_power_meter->WphB > 0) ? ac_power_meter->WphB : 0;//微逆电表
//            EmsPrivate.AC_PVPower[2] = (ac_power_meter->WphC > 0) ? ac_power_meter->WphC : 0;
//    }
//    else
//    {
//        ESP_LOGE(TAG, "Error:ACMeter:ID = %d",  ac_power_meter->ID);
//        p_EmsPublic->InvDetail.AC_PvPower[0] = 0;//0
//        p_EmsPublic->InvDetail.AC_PvPower[1] = 0;//0
//        p_EmsPublic->InvDetail.AC_PvPower[2] = 0;//0
//
//        EmsPrivate.AC_PVPower[0] = 0;
//        EmsPrivate.AC_PVPower[1] = 0;
//        EmsPrivate.AC_PVPower[2] = 0;
//    }

    //p_EmsPublic->InvDetail.InvUpgradeFlag = ;//？
    p_EmsPublic->InvDetail.PvLimitFlag =  inv_announce->overall_status&0x00F0;//bit7/4:光伏受限标志,0-无pv 1-存在PV且未达到最大功率点2-PV功率已达最大;
    //TimeEnable gTimeEnable;
    return 1;
}



// 补偿最大值分档位
static float calculate_max_output(uint16_t target_power) {
    if (target_power > 800) {
        return 80;
    } else if (target_power >= 100) {
        return 50;
    } else if(target_power >= 20){
        return 20;
    } else {
        return 0;
    }
}




/* 更新补偿功率的函数

return: 调节误差
*/
static int16_t update_compensa_power(int32_t target_power, int32_t actual_power, uint8_t i) 
{
	static uint8_t oneflag = 0;
	uint8_t num = 0;	
	if(0 == oneflag)
	{
		oneflag =1;
	//    for (uint8_t i = 0; i < 3; i++) 
		num =0;
		{
			// PID控制器参数 
			ems_pid[num].Kp = 0.1; // 比例系数
			ems_pid[num].Ki = 0.1; // 积分系数
			ems_pid[num].Kd = 1; // 微分系数
			
			// PID控制器状态
			ems_pid[num].integral = 0;
			ems_pid[num].last_error = 0;
			ems_pid[num].derivative_filtered = 0;
			ems_pid[num].alpha = 0.1;
		}
		i =1;
		{
			// PID控制器参数 
			ems_pid[num].Kp = 0.1; // 比例系数
			ems_pid[num].Ki = 0.1; // 积分系数
			ems_pid[num].Kd = 1; // 微分系数
			
			// PID控制器状态
			ems_pid[num].integral = 0;
			ems_pid[num].last_error = 0;
			ems_pid[num].derivative_filtered = 0;
			ems_pid[num].alpha = 0.1;
		}
		i =2;
		{
			// PID控制器参数 
			ems_pid[num].Kp = 0.1; // 比例系数
			ems_pid[num].Ki = 0.1; // 积分系数
			ems_pid[num].Kd = 1; // 微分系数
			
			// PID控制器状态
			ems_pid[num].integral = 0;
			ems_pid[num].last_error = 0;
			ems_pid[num].derivative_filtered = 0;
			ems_pid[num].alpha = 0.1;
		}	
	}


    float error = target_power - actual_power; // 计算误差
    float max_output = calculate_max_output(target_power); // 计算最大输出功率
    ems_pid[i].integral += error; // 更新积分项
    if(ems_pid[i].integral > max_output/ems_pid[i].Ki)
	{ // 抗积分饱和
        ems_pid[i].integral = max_output/ems_pid[i].Ki;
    } 
	else if(ems_pid[i].integral < 0)
	{
        ems_pid[i].integral = 0;
    }
    float current_derivative = error - ems_pid[i].last_error; // 计算当前微分项，未经滤波
    float derivative = ems_pid[i].alpha * current_derivative + (1 - ems_pid[i].alpha) * ems_pid[i].derivative_filtered; // 应用低通滤波
    ems_pid[i].derivative_filtered = derivative; // 更新滤波后的微分项，用于下一次迭代
    ems_pid[i].last_error = error; // 更新上一次的误差
    // 计算PID控制器的输出
    float output = ems_pid[i].Kp*error + ems_pid[i].Ki*ems_pid[i].integral + ems_pid[i].Kd*derivative;
    // 限制输出功率的范围
    if (output > max_output)
	{
        output = max_output;
    } 
    else if (output < 0)
	{
        output = 0;
    }

    ESP_LOGW(TAG, "target_power:%ld, actual_power:%ld, output:%f, Kp_power:%f, Ki_power:%f, Kd_power:%f", 
            target_power, actual_power, output, ems_pid[i].Kp*error, ems_pid[i].Ki*ems_pid[i].integral, ems_pid[i].Kd*derivative);
    return (int16_t)output;
}

//#if 1
// 单个插座掉线后2min内保持上一次的值，多个插座中的一个掉线不做处理 （针对弱网环境，问题：如果用户拔掉插座会导致输出上一次的值而非0） 
static uint16_t check_smplug_valid(uint16_t load_power)
{
    static uint16_t last_load_power = 0;
    static uint16_t disconnect_count = 0;
    uint16_t out_load_power = 0;
//    if(smplug_data.plug_valid == 0)
		if(0)
	{ 
        disconnect_count++;
        if(disconnect_count < 120){
            out_load_power = last_load_power;
        } else{
            last_load_power = 0;
            out_load_power = 0;
        }
    } 
	else
	{
        last_load_power = load_power;
        out_load_power = load_power;
        disconnect_count = 0;
    }
    return out_load_power;
}
//#endif
static uint16_t time_set_check(void)
{
    struct tm tm;
    time_t now;
    now = time(NULL);
    localtime_r(&now, &tm);

    ESP_LOGI(TAG, "secs=%d, mins=%d, hours=%d, "
        "mday=%d, mon=%d, year=%d, wday=%d\n",
        tm.tm_sec, tm.tm_min, tm.tm_hour,
        tm.tm_mday, tm.tm_mon, tm.tm_year, tm.tm_wday);

    uint32_t now_second = tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec;
    for (uint8_t i = 0; i < TIME_CTRL_NUM; i++) 
	{
        uint32_t ctrl_start = Plug[PLUG_MAX_NUM].mod_reg02000_Inv_base_set.ctrl_time[i].start.minutes * 60 + 
                                Plug[PLUG_MAX_NUM].mod_reg02000_Inv_base_set.ctrl_time[i].start.hours * 3600;   
        uint32_t ctrl_end   = Plug[PLUG_MAX_NUM].mod_reg02000_Inv_base_set.ctrl_time[i].end.minutes * 60 + 
                                Plug[PLUG_MAX_NUM].mod_reg02000_Inv_base_set.ctrl_time[i].end.hours * 3600;
        
        ESP_LOGW(TAG,"now_second:%lu,ctrl_start:%lu,ctrl_end:%lu,label:%hu", now_second, ctrl_start, ctrl_end,
                Plug[PLUG_MAX_NUM].mod_reg02000_Inv_base_set.ctrl_time[i].lable);

        if (now_second >= ctrl_start && now_second <= ctrl_end) {
            return Plug[PLUG_MAX_NUM].mod_reg02000_Inv_base_set.ctrl_time[i].lable;
        }
    }
    return 0; // 未设置时间段默认负载功率为0W
}





/*
IOT CAN set INV AC输出目标值
Inv_index:并机序号;0xFF-广播；1/2/3-各机
ACPhase_cnt:1/2/3相

TargetPower[3]:3相设置目标功率


*/
int8_t SettingTargetPower(uint8_t Inv_index, uint16_t ACPhase_cnt, int16_t  *TargetPower)
{
	uint8_t dst=0;
	IdStruct canid;
    int8_t  RET_SUCCESS = 1;
    can_ems_t can_ems = {0};

	if(Inv_index < 16)
	{
		dst = INV_CAN_ADDR + Inv_index;
	}
	else
	{
		dst = 0xFF;
	}
	
    // if(1 == ACPhase_cnt)// 
	{
		/* 填充CANID数据结构: 0x1901xxXX */
		canid.bit.src = esp_canbus_myself_address();
		canid.bit.dst = dst;//CAN_BROADCAST_ADDRESS;
		canid.bit.funcode = CAN_REPORT_FRAME_FUNC_CODE;
		canid.bit.page = CAN_REPORT_FRAME_PAGE;
		canid.bit.priority = CAN_REPORT_FRAME_PRIORITY;

		can_ems.power_l1 =TargetPower[0];
		can_ems.power_l2 = TargetPower[1];
        can_ems.power_l3 = TargetPower[2];

		/* 发送单帧CAN数据 */
		if (!CanAckData(0, canid.all, (uint8_t *)&can_ems, sizeof(can_ems_t))) 
		{
			RET_SUCCESS = -1;
		}
		can_ems.all = 0;
		}
    return RET_SUCCESS;
}

/*
A.AC380 ARM做主， IOT提供 各相所有负载功率发到CAN总线（下发EMS CTRL1/2/3:0x08FAFFXX 11/12/13-总各相限制功率目标值；或新增S1 负载汇总变量到CAN总线），AC380处理各INV分配；

暂定 IOT发EMS CTRL1/2/3:0x08FAFFXX 11/12/13-总各相限制功率目标值 ==S1 load sum
*/
void system_ems_handle_A(void) //1s cycle
{

    uint8_t i=0;  
    uint32_t SmartPlug_Power =0;//
    uint32_t PowerSum_Acseq_S1[3];

    static uint8_t sdelay=0;  
    // modify by debug:5-->2 200ms
	if((++sdelay >= 50) && (reals.ota_happen != 1))//S1 5s周期上报 OTA升级时禁止发送 待增加变化后立即上报(哪个值？)
	{
		sdelay=0;
	}
	else
	{
		return;
	}

	for(i = 0; i < 3; i++)// 
	{
		PowerSum_Acseq_S1[i]=0;
	}



	
	for(i = 0; i < PLUG_MAX_NUM; i++)// 
	{
        // ESP_LOGI(TAG, "SmartPlug[%d] SmartPlug_States = %d", i, Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_States.all);
        uint8_t plug_ac_phase = Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_States.ac_phase_en;
        if (0 == plug_ac_phase)
        {
            // ESP_LOGW(TAG, "SmartPlug[%d] AC Phase setting is invalid", i);
            continue;
        }

		SmartPlug_Power += Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_Power;
		if(1 == plug_ac_phase)//L1
		{
			PowerSum_Acseq_S1[0] += Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_Power;
		}
		else if(2 == plug_ac_phase)//L2
		{
			PowerSum_Acseq_S1[1] += Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_Power;
		}
		else if(3 == plug_ac_phase)//L3
		{
			PowerSum_Acseq_S1[2] += Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_Power;
		}
	}
	
	Plug[PLUG_MAX_NUM].mod_reg14500_SmartPlug_info.SmartPlug_Power =SmartPlug_Power;
	
	for(i = 0; i < 3; i++)// 
	{
		reals.PowerSum_Acseq_S1[i] = PowerSum_Acseq_S1[i]/10;
		ESP_LOGI(TAG, "reals.PowerSum_Acseq_S1[%d]  =%d",i,reals.PowerSum_Acseq_S1[i]);
		
	}

	SettingTargetPower(0xFF, 1, reals.PowerSum_Acseq_S1);
}


/*
IOT做主，IOT基于S1 WIFI负载数据直接计算，然后发 CAN命令 EMS CTRL1/2/3:0x08FAFFXX 11/12/13-各相限制功率目标值给各个INV


*/
void system_ems_handle_B(void) //1s cycle  tbd 未完成
{
    int cmd_len = 0;
    static uint8_t sdelaycnt=0;  
    uint8_t i=0;  
    uint8_t inv_index=0;  
	
    uint16_t  work_mode;
    int32_t load_power =  0;
    int16_t i16_temp_data =  0;
    int16_t compensa_power;
    int16_t running_power;

	if(++sdelaycnt >= 6)//6s cycle
	{
		sdelaycnt =0;

	}
	else
	{
		return;
	}
	
	
/*
取INV定义，而非阳台光伏定义
INV:SetCtrlWorkMode  2005 

00：默认（无工作模式）；01：高级(用户自定义模式)；02：经济模式；03：UPS在线；04：UPS后备；05：峰谷；06：离网；


阳台光伏：
	当其为阳台光伏时，
	00：默认（无工作模式）；
	1.AUTO模式（搭配智能插座）
	2.手动设置 3.自定义模式


	*/
	
//	Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02200_Inv_advance_set.ctrl_feedback_max_power
//	Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02200_Inv_advance_set.ctrl_feedback
//
//	Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02200_Inv_advance_set.CounterCurrentPower_Limit
	work_mode =Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02000_Inv_base_set.work_mode;
//Plug[PLUG_MAX_NUM].mod_reg02000_Inv_base_set.work_mode
	for(inv_index = 0; inv_index < MAXPARLLELNUM; inv_index++)//phase
	{
		for(i = 0; i < 3; i++)//phase
		{
			if(2 == work_mode)// 阳台光伏：1-自动模式，和插座联动;INV:02-经济模式
			{
			
				load_power = Plug[0].mod_reg01300_Inv_grid.grid_detail[i].input_power;
				load_power = check_smplug_valid(load_power); // 检查插座是否掉线
	//			compensa_power = get_compensa_power(load_power);
				running_power = Inv[0].mod_reg01300_Inv_grid.grid_detail[i].input_power;//INV实时总放电功率
				if(running_power < 0)
				{
					running_power =0;
				}
				compensa_power = update_compensa_power(load_power, running_power, i);
		
				EmsPublic[inv_index].TargetPower[i] += compensa_power;
			
			}
			else if(2 == work_mode)// 手动模式，基于APP设置值
			{
				if(Inv[0].mod_reg02000_Inv_base_set.P_inv_active_target_L[i] < 0)
				{
					i16_temp_data=0;
				}
				else
				{
					i16_temp_data = Inv[0].mod_reg02000_Inv_base_set.P_inv_active_target_L[i];
				}
				load_power += i16_temp_data;	
			
	//			compensa_power = get_compensa_power(load_power);
				running_power = Inv[0].mod_reg01300_Inv_grid.grid_detail[i].input_power;//INV实时总放电功率
				if(running_power < 0)
				{
					running_power =0;
				}
				compensa_power = update_compensa_power(load_power, running_power, i);
				
				EmsPublic[inv_index].TargetPower[i] += compensa_power;
			
			}
			else if(3 == work_mode)// 自定义模式,TOU
			{
				load_power = time_set_check();
	//			compensa_power = get_compensa_power(load_power);
				running_power = Inv[0].mod_reg01300_Inv_grid.grid_detail[i].input_power;//INV实时总放电功率
				if(running_power < 0)
				{
					running_power =0;
				}
				compensa_power = update_compensa_power(load_power, running_power, i);
				
				EmsPublic[inv_index].TargetPower[i] += compensa_power;
			
			}
		}
	}



	

	//先计算出每台机器的目标功率，然后一并放入CAN下发队列
	for(i = 0; i < EmsSetParameter.Online_Count; i++)
	{
		if((EmsSetParameter.MutiInvEnable == 1) &&		
		   (EmsPublic[i].InvDetail.ParallelInvState >= 2)&&(EmsPublic[i].InvDetail.ParallelInvState != 6)  
//						   &&(INVPhaseMaxChgPowerFlag == 1)
			)
		{
//							Debug_Power_Info(1,Inv_Index);
			SettingTargetPower(i,EmsPrivate.SysPhase, &EmsPublic[i].TargetPower[0]);
		}
	}

	
//    ESP_LOGW(TAG, "work_mode:%d,load_power:%ld", work_mode, load_power);
}


//#define	 AC380_EMS_IOT_MASTER_ENABLE	//使能-IOT做EMS策略；禁止-AC380 ARM做 EMS策略
/*
AC380和S1组合防逆流的两种工作分配方法：
A.AC380 ARM做主， IOT提供 各相所有负载功率发到CAN总线（下发EMS CTRL1/2/3:0x08FAFFXX 11/12/13-总各相限制功率目标值；或新增S1 负载汇总变量到CAN总线），AC380处理各INV分配；
B.AC380 IOT做主，IOT基于S1 WIFI负载数据直接计算，然后发 CAN命令 EMS CTRL1/2/3:0x08FAFFXX 11/12/13-各相限制功率目标值给各个INV；
暂定A方案

*/
void system_ems_handle(void) //1s cycle
{
#ifdef	AC380_EMS_IOT_MASTER_ENABLE//
	system_ems_handle_B();
#else//INV ARM做EMS策略
	system_ems_handle_A();

#endif

}


//
//
///*------------------------------------------------------------------------------
// Function: Grid_Forbid_Back_Ctrl_Cmd
// -----------------------------------------------------------------------------*/
///**
//  * @brief      电网防逆流
//  * @param[in]  void  
//  * @param[out] None
//  * @return     void
//  */
//void Grid_Forbid_Back_Ctrl_Cmd(void)//1s cycle
//{ 
//  	uint16_t i;
////  uint16_t j;
//    uint8_t slaveaddress;
//    uint8_t Inv_Index=0;  
//
//	uint16_t Tempdata_I=0;
//	uint16_t Tempdata_P=0;
//    uint16_t Tempdata_Energy=0;
//	int16_t Tempdata_level = SetData.dev_info_t.CounterCurrentPower_Limit;//Grid防逆流功率阈值
//	int16_t Tempdata_sum=0;//
//
//	uint8_t net_point_Num_temp=0;//在线的设备节点
//	static uint16_t scnt_1s=0;
//   
//	for (i = 0; i < (NET_SUB1G_MAX_POINT); i++)// 
//	{	
//	    if(NET_POINT_ONLINE == reals.net_point_base_Info[i].net_point_online)
//        {   
//    		Tempdata_P +=MicroInv[i+1].mod_reg01300_Inv_grid.grid_detail[0].input_power;
//    		Tempdata_I +=MicroInv[i+1].mod_reg01300_Inv_grid.grid_detail[0].input_current;
//            Tempdata_Energy +=MicroInv[i+1].mod_reg01200_Inv_pv.total_chg_energy;
//
//    		reals.net_point_power[i].P_set_level =MicroInv[i+1].mod_reg02500_Inv_advance_set2.set_inv_max_power;
////            reals.net_point_power[i].P_set_level =MicroInv[i+1].mod_reg02000_Inv_base_set.P_inv_active_target_L1;
//    		reals.net_point_power[i].P_real =MicroInv[i+1].mod_reg01300_Inv_grid.grid_detail[0].input_power;
//    		reals.net_point_power[i].Pdelta =(int16_t)reals.net_point_power[i].P_set_level -(int16_t)reals.net_point_power[i].P_real;
//    		reals.net_point_power[i].Pability_remain =INV_MAX_RATE_OUTPUT - reals.net_point_power[i].P_set_level;//
//        }
//	}
//	
//	MicroInv[0].mod_reg01500_Inv_inv.inv_detail[0].current =Tempdata_I;
//	MicroInv[0].mod_reg01500_Inv_inv.inv_detail[0].power =Tempdata_P;//micro INV实际报文从1300上报的AC INFO；要汇总到1500 INV:1300的汇总为grid；max 63kw,tbd
//	MicroInv[0].mod_reg01500_Inv_inv.total_energy = Tempdata_Energy;	
//
//
///*
//外部因素：
//每个微逆的P_real和P_set，受限于阳光遮挡强度，大概率会出现P_set远大于P_real情况
//
//增加AC发电策略：
//只调节P_real和P_set差值小(<100w)的设备
//
//减小AC发电策略：
//基于P_real 调节
//*/
//
//	if(++scnt_1s >= 7)
//	{
//		scnt_1s =0;
//
//        if ((reals.GridAddr_done == 0) 
////			|| (IotSetData.iot_dev_info_t.ctrl_meter.ctrl_meter_enable != enable)
//			)//电表未连接或电表故障
//        {
//            memset(&MicroInv[0].mod_reg01300_Inv_grid, 0, sizeof(MicroInv[0].mod_reg01300_Inv_grid));
////            ESP_LOGE(TAG, "[Grid_Forbid_Back_Ctrl_Cmd] GridAddr_done = 0");
//            return;
//        }
//		
//
//        if(
//			(1 != SetData.dev_info_t.ctrl_feedback)     //禁止馈电 SetData_Can.dev_info_t2.inv_set01.ctrl_feedback
////            && (IotSetData.iot_dev_info_t.Meter_Select == 1))       //入户侧电表
//        {
//    		Tempdata_sum = MicroInv[0].mod_reg01300_Inv_grid.total_chg_power + Tempdata_level;
//			for (i = 0; i < NET_SUB1G_MAX_POINT; i++)// 
//    		{
//    		    if(NET_POINT_ONLINE == reals.net_point_base_Info[reals.Pseq_index[i]].net_point_online)
//                {  
//                    slaveaddress = reals.Pseq_index[i] + 1;
//                    
//        			if(Tempdata_sum > INV_RATE_ERROR_RANGE)//待降低微逆功率
//        			{
//        			    ESP_LOGE(TAG, "(Tempdata_sum + Tempdata_level) > 0");
//                        
//        				//基于实际功率减小
//        				if(Tempdata_sum >  reals.net_point_power[reals.Pseq_index[i]].P_real )// 
//        				{
//        				    if(reals.net_point_power[reals.Pseq_index[i]].P_real > INV_MIN_OUTPUT)//实际功率低于最低设置功率时不再调整
//                            {            
//            					MicroInv_WR[slaveaddress].mod_reg02500_Inv_advance_set2.set_inv_max_power = (uint16_t)INV_MIN_OUTPUT; 
//            					Tempdata_sum -= reals.net_point_power[reals.Pseq_index[i]].P_real - MicroInv_WR[slaveaddress].mod_reg02500_Inv_advance_set2.set_inv_max_power;
////                                MicroInv_WR[slaveaddress].mod_reg02000_Inv_base_set.P_inv_active_target_L1 = (uint16_t)INV_MIN_OUTPUT; 
////            					Tempdata_sum -= reals.net_point_power[reals.Pseq_index[i]].P_real - MicroInv_WR[slaveaddress].mod_reg02000_Inv_base_set.P_inv_active_target_L1;
//                            }
//                            else
//                            {
//                                continue;
//                            }
//        				}
//        				else//
//        				{
//        				    if((reals.net_point_power[reals.Pseq_index[i]].P_real -Tempdata_sum) > INV_MIN_OUTPUT)//最后一个待减少设备
//                            {               
//            					MicroInv_WR[slaveaddress].mod_reg02500_Inv_advance_set2.set_inv_max_power =reals.net_point_power[reals.Pseq_index[i]].P_real -Tempdata_sum;
////                                MicroInv_WR[slaveaddress].mod_reg02000_Inv_base_set.P_inv_active_target_L1 =reals.net_point_power[reals.Pseq_index[i]].P_real -Tempdata_sum;
//            					Tempdata_sum = 0;
//                            }
//                            else
//                            {
//                                MicroInv_WR[slaveaddress].mod_reg02500_Inv_advance_set2.set_inv_max_power = INV_MIN_OUTPUT;
//                                Tempdata_sum -= reals.net_point_power[reals.Pseq_index[i]].P_real - MicroInv_WR[slaveaddress].mod_reg02500_Inv_advance_set2.set_inv_max_power;
////                                MicroInv_WR[slaveaddress].mod_reg02000_Inv_base_set.P_inv_active_target_L1 = (uint16_t)INV_MIN_OUTPUT; 
////                                Tempdata_sum -= reals.net_point_power[reals.Pseq_index[i]].P_real - MicroInv_WR[slaveaddress].mod_reg02000_Inv_base_set.P_inv_active_target_L1;
//                            }
//        				}	
//        				
//        			}
//        			else if((- Tempdata_sum) > INV_RATE_ERROR_RANGE)//待增加微逆功率,<0
//        			{
//        			    ESP_LOGE(TAG, "(Tempdata_sum + Tempdata_level) < 0 ");
//        				//基于set功率增加
//        				if((-Tempdata_sum) >  reals.net_point_power[reals.Pseq_index[i]].Pability_remain )// 
//        				{
//        					MicroInv_WR[slaveaddress].mod_reg02500_Inv_advance_set2.set_inv_max_power = MicroInv[slaveaddress].mod_reg02500_Inv_advance_set2.set_inv_max_power + reals.net_point_power[reals.Pseq_index[i]].Pability_remain;
////                            MicroInv_WR[slaveaddress].mod_reg02000_Inv_base_set.P_inv_active_target_L1 = MicroInv[slaveaddress].mod_reg02000_Inv_base_set.P_inv_active_target_L1 + reals.net_point_power[reals.Pseq_index[i]].Pability_remain;
//        					Tempdata_sum +=reals.net_point_power[reals.Pseq_index[i]].Pability_remain;
//        				}	
//        				else//最后一个待增加设备
//        				{
//        					MicroInv_WR[slaveaddress].mod_reg02500_Inv_advance_set2.set_inv_max_power = MicroInv[slaveaddress].mod_reg02500_Inv_advance_set2.set_inv_max_power + (-Tempdata_sum);
////                            MicroInv_WR[slaveaddress].mod_reg02000_Inv_base_set.P_inv_active_target_L1 = MicroInv[slaveaddress].mod_reg02000_Inv_base_set.P_inv_active_target_L1 + (-Tempdata_sum);
//        					Tempdata_sum = 0;
//        				}		
//        			}
//        			else//回差不调整，tbd
//        			{
//        		        ESP_LOGE(TAG, "(Tempdata_sum + Tempdata_level) = %d",Tempdata_sum);
//                        return;
//        			}
//        		
////        		//给微逆下发设置命令
////        		
////        			reg_position_list_t *new_position = (reg_position_list_t *)heap_caps_malloc(sizeof(reg_position_list_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
////        			if (new_position != NULL) 
////        			{
////        				new_position->next = NULL;
////        				new_position->dev_index = slaveaddress;//SlaveAddress;
//////        				new_position->position.reg_addr = 2500;//2505 reg_position.reg_addr;
//////        				new_position->position.offset	= 5<<1;//reg_position.offset;
////        				new_position->position.reg_addr = 2000;//2068 reg_position.reg_addr;
////        				new_position->position.offset	= 68<<1;//reg_position.offset;
////        				new_position->position.len	= 1<<1;//reg_position.len;
////        				md_data_CallBack_run(new_position->dev_index, new_position->position.reg_addr, new_position->position.len>>1);
////        				ESP_LOGI(TAG, "dev_index: %02x, reg_num: %02x, offset: %02x, bytes: %02x", 
////        									(new_position)->dev_index, (new_position)->position.reg_addr,
////        									(new_position)->position.offset, (new_position)->position.len);
////        				sys_new_position_and_transmit(new_position);//透传转发给下级uart
////        			}
//
//					//先计算出每台机器的目标功率，然后一并放入CAN下发队列
//					for(Inv_Index = 0; Inv_Index < EmsSetParameter.Online_Count; Inv_Index++)
//					{
//						if((EmsSetParameter.MutiInvEnable == 1) &&		
//						   (EmsPublic[Inv_Index].InvDetail.ParallelInvState >= 2)&&(EmsPublic[Inv_Index].InvDetail.ParallelInvState != 6)  
////						   &&(INVPhaseMaxChgPowerFlag == 1))
//						{
////							Debug_Power_Info(1,Inv_Index);
//							SettingTargetPower(Inv_Index);
//						}
//					}
//
//
//
//					
//                }
//    		}
//        }
//	}
//}

/*
INV和 S1 防止逆流逻辑策略
*/
void ems_stop_reverse(void)
{
    uint8_t i =0;

    for(i = 0; i < EmsSetParameter.Online_Count; i++)//M104 MAXPARLLELNUM
    {
        EMSPublicDataInput(i);
    }

	system_ems_handle();

}



