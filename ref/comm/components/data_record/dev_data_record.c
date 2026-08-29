/**
  ******************************************************************************
  * @file      dev_data_record.c
  * @version   1.0
  * @author    lixingyu
  * @date      2025/1/6
  * @brief     历史告警记录数据操作
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/1/6   <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#include "dev_data_record.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#include "can_protocol.h"
#include "filesystem.h"
#include <math.h>
#include "esp_log.h"
#include "tou_relay_ctrl.h"
#include "iot_box_task.h"
#include "modbus_data.h"
#include "ota_define.h"
#include "sync_time.h"

static const char *TAG = "[DEV_DATA_RECORD]";

EXT_RAM_BSS_ATTR name_list_t2 *fsys_file_list;		// 文件目录链表中文件名称的指针;从小到大;[x]-链表元素的顺序


//modbus 3000历史记录
QueueHandle_t xQueue_Log_record = NULL;//CAN底层 Rx队列
#define HISTORY_MAX_COUNT 		 (HISTORY_LOG_RECORD_MAX_NUM)	//最大历史记录数
#define HISTORY_ONE_BYTE_COUNT 	 (sizeof(reals.inv_log_fault_info)) //一条历史记录的存储字节长度，10B

QueueHandle_t xQueue_iot_record_event = NULL;//CAN底层 Rx队列

// IOT事件日志文件操作互斥信号量
static SemaphoreHandle_t xIoTEventLogFileProcessSemaphore = NULL;

// 逆变告警事件日志文件操作互斥信号量
static SemaphoreHandle_t xInvLogFileProcessSemaphore = NULL;

static USE_EXT_RAM_BSS EventHistoryData_Struct HistoryData_event;//iot事件记录
/**
 * @brief 序列化历史数据查询指令
 *
 * 该函数将查询到的历史数据文件名 序列化为协议中指定的数据格式

 序列化上报之前，需要RAM 文件名基于时间u16排序
 *
 * @param file_query 待序列化的文件查询指针
 * @param/output: buf 用于保存序列化后的数据缓存
 *
 * @return 返回序列化后的数据长度
 */
uint32_t historic_record_serialize_query( uint8_t *buf)
{
    uint32_t idx = 0;
    uint16_t i =0;

    if(buf == NULL) 
    {
        return 0;
    }   
    
    /* 序列化查询到的文件名 */
    for (i = 0; i < reals.file_nums; i++)// 
    {        
        memcpy(&buf[idx], fsys_file_list[i].name, FILE_DIR_LEN);
        idx += FILE_DIR_LEN;

        /*逗号分隔*/
        buf[idx] = 0x2C;
        idx++;
    }
    
    return idx;
}



/**
将存储在结构体中的文件名序列，按照从小到大顺序排列，便于上级上报查询

 *
 * @return 返回序列化后的数据长度
 */
void Filename_Compare_Sequence( void)
{
	uint16_t i =0;
	uint16_t j =0;
	name_list_t2 temp;

	/* 序列化查询到的文件名 */
    if(reals.file_nums > 1)
    {
        for (i = 0; i < (reals.file_nums -1); i++)// 
        { 
            for (j = 1; j < (reals.file_nums - i); j++)// 
            { 	  
                if( (0 != fsys_file_list[j].filename_value)//排除空节点
                 &&(fsys_file_list[j-1].filename_value > fsys_file_list[j].filename_value)//相邻两个数如果逆序，则交换位置(从小到大排序)
                 )
                {
                //  内容排序			  
                    memcpy(&temp, &fsys_file_list[j-1], sizeof(name_list_t2));
                    memcpy(&fsys_file_list[j-1], &fsys_file_list[j], sizeof(name_list_t2));
                    memcpy(&fsys_file_list[j], &temp, sizeof(name_list_t2));
                }
            }
        }
    }
}


/**
 * @brief 历史数据文件目录检查
 *
 * 该函数检查历史数据的目录是否存在,如果不存在则创建目录
 *
 * @param fname 待写入的文件名,名称中包含除根目录的文件路劲
 *
 * @return none
 */
void historic_dir_check(char *fname)
{
	char name[strlen(fname)+1];
	strcpy(name, fname);

	char dir_path[100] = {0};
	sprintf(dir_path, "%s", RECORD_ROOT_PATH"/");

    char *p;
    char *token = strtok_r(name, "/", &p);

	/* 检查路劲中的目录是否存在 */
    while(token != NULL)
    {
		char buffer[strlen(token)+1];
		strcpy(buffer, token);

		token = strtok_r(NULL, "/", &p);
		if(token != NULL)
		{
			strcat(dir_path, buffer);

			DIR *dir = opendir(dir_path);
			if(dir == NULL) 
            {   
                //目录不存在则创建目录
			    ESP_LOGE(TAG, "[historic_dir_check] dir == NULL (%s)",dir_path);
				mkdir(dir_path, 0777);
			}
			else {
				closedir(dir);
			}

			strcat(dir_path, "/");
		}
    }

	return;
}

/**
 * @brief 保存历史数据
 *
 * 历史数据按照每条方式顺序保存在文件中,该函数从文件末尾顺序写入多条数据
 *
 * @param fname 待写入的文件名,名称中包含除根目录的文件路劲
 * @param record 历史数据指针
	uint32_t file_bias:指定操作的偏移条数：0-从头开始；0xFFFF-从尾部追加；其他变量-从偏移位置开始覆盖写操作 :file_bias*len
 * @param len :一条历史数据长度
 *
 * @return 成功返回0,否则返回-1

两种写入方法
 尾部追加写入；
 或指定中间偏移写入
 */
int historic_data_write(char *fname, void *record, uint32_t file_bias, uint32_t len)
{
	int fd;

	if(fname == NULL || record == NULL || len == 0) return -1;

	char path[30] = {0};
    
	sprintf(path, "%s/%s", RECORD_ROOT_PATH, fname);

	/* 检查历史数据目录 */
	historic_dir_check(fname);

	if(0xFFFF == file_bias)//从文件末尾写入数据
	{
		/* 只写格式打开文件,不存在时创建,
		 * 每次从文件末尾写入数据,且在写过程中使用lseek移动文件指针无效 */
		 fd = open(path, O_WRONLY|O_CREAT|O_APPEND);
		if(fd < 0) 
		{
			return -1;
		}	
	}
	else//从偏移位置写入数据
	{
		/* 只写格式打开文件,不存在时创建,
		 * 每次从文件末尾写入数据,且在写过程中使用lseek移动文件指针无效 */
		 fd = open(path, O_WRONLY|O_CREAT);
		if(fd < 0) 
		{
			return -1;
		}	
			/* 移动文件指针到设置的数据位置 */
        uint32_t file_pos = file_bias * len;
		if(lseek(fd, file_pos, SEEK_SET) != file_pos) 
		{
			goto __error;
		}
	}


	/* 写历史数据 */
	if(write(fd, record, len) != len)
	{
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
__error:
	close(fd);
	return -1;	
}

/**
 * @brief 读取历史数据大小
 *
 * 历史数据按照每条方式顺序保存在文件中,该函数从指定的位置读取文件剩余数据大小
 *
 * @param/input: fname 待读取的文件名,名称中包含除根目录的文件路劲
 * @param/input: file_pos 数据位置
 * @param/output size 用于保存读取的剩余数据大小
 *
 * @return 成功返回0,否则返回-1
 */
int historic_data_get_size(char *fname, uint32_t file_pos, uint32_t *size)
{
    if(fname == NULL) 
    {
        ESP_LOGE(TAG, "[historic_data_get_size] fname = NULL");
        return -1;
    }

    char path[30] = {0};

#ifdef FILE_SYSTEM_DIRECTORY_ENABLE
    
    if(fname[0] == 0) //查询根目录
    {   
        sprintf(path, "%s/%s", RECORD_ROOT_PATH, &fname[1]);
    }
    else //查询子目录
    {               
        sprintf(path, "%s/%c/%s", RECORD_ROOT_PATH, fname[0], &fname[1]);
    }

#else

    if(fname[0] == 0) //查询根目录
    {   
        sprintf(path, "%s/%s", RECORD_ROOT_PATH, &fname[1]);
    }
    else //查询子目录
    {               
        sprintf(path, "%s/%s", RECORD_ROOT_PATH, &fname[0]);
    }

#endif
    
    ESP_LOGW(TAG, "[historic_data_get_size] read path : %s", path);

    /* 只读格式打开文件 */
    int fd = open(path, O_RDONLY);
    if(fd < 0)
    {
        ESP_LOGE(TAG, "[historic_data_get_size] open(%s) failed: errno=%d", path, errno);
        return -1;
    }

    int32_t start_pos;
    int32_t end_pos;

    if((start_pos = lseek(fd, file_pos, SEEK_SET)) != file_pos) 
    {
        ESP_LOGE(TAG, "[historic_data_get_size] start_pos error (%lu)", file_pos);
        goto __error;
    }

    if((end_pos = lseek(fd, 0, SEEK_END)) < 0) 
    {
        ESP_LOGE(TAG, "[historic_data_get_size] end_pos error (%lu)", end_pos);
        goto __error;
    }

    if(end_pos <= start_pos) 
    {
        ESP_LOGE(TAG, "[historic_data_get_size] pos error (%lu)(%lu)",start_pos, end_pos);
        goto __error;
    }
    
    *size = end_pos - start_pos; 

    ESP_LOGW(TAG, "[historic_data_get_size] read size : %lu", *size);
    
    close(fd);
    return 0;

__error:
    close(fd);
    return -1;
}


/**
 * @brief 读取历史数据
 *
 * 历史数据按照每条方式顺序保存在文件中,该函数从指定的位置读取多条历史数据
 *
 * @param fname 待读取的文件名,名称中包含除根目录的文件路劲
 * @param record ,output buf 历史数据指针
 * @param len 读取的数据长度
 * @param file_pos 数据位置
 *
 * @return 成功返回0,否则返回-1

 函数原型： int lseek(int handle,long offset,long length);
功能：用于移动打开文件的指针
参数：int handle  为要移动文件指针的文件句柄
          long offset 为要移动的偏移量
          int fromwhere 为文件指针以什么方向计算偏移量。
          有三个取值分别为：
          SEEK_SET  文件的开头
          SEEK_CUR 文件的当前位置
          SEEK_END  文件的末尾
返回值：移动文件指针后的文件指针位置
 */
int historic_data_read(char *fname, void *record, uint32_t *len, uint32_t file_pos)
{
    if(fname == NULL || record == NULL || *len == 0) 
    {
        ESP_LOGE(TAG, "[historic_data_read] error");
        return -1;
    }

    char path[30] = {0};
    
#ifdef FILE_SYSTEM_DIRECTORY_ENABLE
    
    if(fname[0] == 0) //查询根目录
    {   
        sprintf(path, "%s/%s", RECORD_ROOT_PATH, &fname[1]);
    }
    else //查询子目录
    {               
        sprintf(path, "%s/%c/%s", RECORD_ROOT_PATH, fname[0], &fname[1]);
    }

#else

    if(fname[0] == 0) //查询根目录
    {   
        sprintf(path, "%s/%s", RECORD_ROOT_PATH, &fname[1]);
    }
    else //查询子目录
    {               
        sprintf(path, "%s/%s", RECORD_ROOT_PATH, &fname[0]);
    }

#endif

    /* 只读格式打开文件 */
    int fd = open(path, O_RDONLY);
    if(fd < 0)
    {
        ESP_LOGE(TAG, "[historic_data_read] open error");
        return -1;
    }

    /* 移动文件指针到设置的数据位置 */
    if(lseek(fd, file_pos, SEEK_SET) != file_pos) 
    {
        ESP_LOGE(TAG, "[historic_data_read] start_pos set error (%lu)", file_pos);
        goto __error;
    }

    /* 读数据到缓存 */
    if(read(fd, record, *len) != *len) 
    {
        ESP_LOGE(TAG, "[historic_data_read] read error");
        goto __error;
    }

    close(fd);
    return 0;

__error:
    close(fd);
    return -1;
}

#if 0

/*------------------------------------------------------------------------------
 Function: historic_data_bias_read
 -----------------------------------------------------------------------------*/
/**
  * @brief      历史记录偏移读取，单独处理
  * @param[in]  char *fname        
                void *record       
                uint32_t *len      
                uint32_t file_pos  
  * @param[out] None
  * @return     int
  */
int historic_data_bias_read(char *fname, uint8_t *record, uint32_t *len, uint32_t file_pos)
{
	if(fname == NULL || record == NULL || *len == 0) 
    {
        ESP_LOGE(TAG, "[historic_data_bias_read] error");
        return -1;
    }

	char path[30] = {0};
    uint16_t file_bias = 0;
    uint16_t log_size = 0;
    uint16_t max_size = 0;
    uint16_t read_size = 0;//读取长度

#ifdef FILE_SYSTEM_DIRECTORY_ENABLE
    
    if(fname[0] == 0) //查询根目录
	{	
		sprintf(path, "%s/%s", RECORD_ROOT_PATH, &fname[1]);
	}
	else //查询子目录
	{				
		sprintf(path, "%s/%c/%s", RECORD_ROOT_PATH, fname[0], &fname[1]);
	}

#else

    if(fname[0] == 0) //查询根目录
	{	
		sprintf(path, "%s/%s", RECORD_ROOT_PATH, &fname[1]);
	}
	else //查询子目录
	{				
		sprintf(path, "%s/%s", RECORD_ROOT_PATH, &fname[0]);
	}

#endif

    //判断当前文件类型，获取对应本地存储偏移
    if (strncmp(&fname[1], IOT_RECORD_FILE_MARK, 3) == 0)//IOT事件
    {
        file_bias = IotSetData.dev_info_t.Event_AddrIndex;
        max_size = HISTORY_MAX_COUNT * sizeof(EventHistoryData_Struct);
        log_size = sizeof(EventHistoryData_Struct);
        ESP_LOGW(TAG, "[iot historic_data_bias_read] %s, bia = %d", fname, file_bias);
    }
    else if (strncmp(&fname[1], INV_RECORD_FILE_MARK, 3) == 0)//INV历史记录事件
    {
        file_bias = IotSetData.dev_info_t.historyAddrIndex;
        max_size = HISTORY_MAX_COUNT * sizeof(reals.inv_log_fault_info);
        log_size = sizeof(reals.inv_log_fault_info);
        ESP_LOGW(TAG, "[inv historic_data_bias_read] %s, bia = %d", fname, file_bias);
    }
#ifdef INV_LOG_DETAILED_INFO_RECORD    
    else if (strncmp(&fname[1], INV_BOX_FILE_NAME, 3) == 0)//黑匣子事件
    {
        file_bias = IotSetData.dev_info_t.invDetailedInfo_AddrIndex[0];
        max_size = INV_DETAILED_INFO_MAX_COUNT * sizeof(Inv_Detailed_Info_Datas);
        log_size = sizeof(Inv_Detailed_Info_Datas);
        ESP_LOGW(TAG, "[inv historic_data_bias_read] %s, bia = %d", fname, file_bias);
    }
#endif    
    else
    {
        ESP_LOGE(TAG, "[historic_data_bias_read] file type error (%s)", fname);
        return -1;
    }

    *len = (*len / log_size) * log_size;
    file_pos = ((file_pos + log_size - 1) / log_size) * log_size;

	/* 只读格式打开文件 */
	int fd = open(path, O_RDONLY);
	if(fd < 0)
    {
        ESP_LOGE(TAG, "[historic_data_bias_read] open error");
        return -1;
    }
    
    /*获取文件大小*/
    off_t fileSize = lseek(fd, 0, SEEK_END); 
    
    /*尚未存储满*/
    if (fileSize < max_size)
    {
        /* 移动文件指针到设置的数据位置 */
        if(lseek(fd, file_pos, SEEK_SET) != file_pos) 
        {
            ESP_LOGE(TAG, "[historic_data_bias_read] start_pos set error (%lu)", file_pos);
            goto __error;
        }
        
        /* 读数据到缓存 */
        if(read(fd, record, *len) != *len) 
        {
            ESP_LOGE(TAG, "[historic_data_bias_read] read error");
            goto __error;
        }
    }
    else
    {
        /*需要获取的历史纪录从当前位置偏移条数*/
        int real_pos = (file_bias * log_size) - *len;
    
        if (real_pos >= 0)
        {
            /* 移动文件指针到设置的数据位置 */
            lseek(fd, real_pos, SEEK_SET);
            
            /* 读数据到缓存 */
            read_size = *len;
            if(read(fd, record, read_size) != read_size) 
            {
                ESP_LOGE(TAG, "[historic_data_bias_read] read error(%s)",path);
                goto __error;
            }
        }
        /*需要拼接*/
        else
        {
            /* 移动文件指针到设置的数据位置 */
            lseek(fd, real_pos, SEEK_END);
            
            /* 读数据到缓存 */
            read_size = abs(real_pos);
            if(read(fd, record, read_size) != read_size) 
            {
                ESP_LOGE(TAG, "[historic_data_bias_read] read error(%s)",path);
                goto __error;
            }
            
            /* 移动文件指针到设置的数据位置 */
            lseek(fd, 0, SEEK_SET);
            
            /* 读数据到缓存 */
            read_size = *len - abs(real_pos);
            if(read(fd, record + abs(real_pos), read_size) != read_size) 
            {
                ESP_LOGE(TAG, "[historic_data_bias_read] read error(%s)",path);
                goto __error;
            }
        }
    }

	close(fd);
	return 0;

__error:
	close(fd);
	return -1;
}

#endif

/**
 * @brief 查询历史数据文件与文件夹
 从链表，改为不定长指针
 *
 * 该函数将会查询目标目录下的文件与文件夹,文件与目录分别使用链表数据结构保存,
 * 第一次调用时会分配内存空间以保存文件与目录数据,后续调用会释放上一次的内存空间后,重新分配新的空间来保存文件与目录数据
 *
 * @param fname/input 待查询的目录或文件名,名称中包含除根目录的目录路劲
 0-查询根目录
 非0-查询子目录
 * @param query 用于指向查询到的数据结构
 *
 * @return 成功返回0,否则返回-1
 */
int filename_list_get(char *fname)//historic_data_file_get
{
    uint32_t file_nums =0;  

    if(fname == NULL) return -1;

    char path[30] = {0};
    char path2[30] = {0};
    char folder[10] = {0};
    DIR *dir;
    DIR *dir2;
    struct dirent *p_dir;
    struct dirent *p_dir2;
    
#ifdef FILE_SYSTEM_DIRECTORY_ENABLE
        
    if(fname[0] == 0) //查询根目录
    {   
        sprintf(path, "%s", RECORD_ROOT_PATH);
    }
    else //查询子目录
    {               
        sprintf(path, "%s/%s", RECORD_ROOT_PATH, fname);
    }

#else
    
    sprintf(path, "%s", RECORD_ROOT_PATH);
    
#endif

    dir = opendir(path);
    if(dir == NULL) 
    {
        ESP_LOGE(TAG, "[filename_list_get] opendir error(%s)",path);
        return -1;
    }


///step 1:获取文件数量
    while((p_dir = readdir(dir)) != NULL)//遍历指定目录路径下的所有文件，包含子目录的子文件
    {
        ESP_LOGW(TAG, "[filename_list_get] path : %s/%s , d_type : %d", path, p_dir->d_name, p_dir->d_type);
        
        if(strcmp(p_dir->d_name,".")==0 || strcmp(p_dir->d_name,"..")==0) 
        { // 是当前目录或父目录则continue
            continue;
        }

        if(p_dir->d_type == DT_DIR)//目录名
        {
            memcpy(folder, p_dir->d_name, sizeof(folder));
            sprintf(path2, "%s/%s", RECORD_ROOT_PATH, folder);
            dir2 = opendir(path2);
            if(dir2 == NULL) 
            {
                ESP_LOGE(TAG, "[filename_list_get] opendir2 error(%s)",path2);
                continue;
            }
            while((p_dir2 = readdir(dir2)) != NULL)//遍历指定目录路径下的所有文件，不包含子目录的子文件
            {
                ESP_LOGW(TAG, "[filename_list_get] path : %s/%s , d_type : %d", path2, p_dir2->d_name, p_dir2->d_type);
                
                if(strcmp(p_dir2->d_name,".")==0 || strcmp(p_dir2->d_name,"..")==0) 
                { // 是当前目录或父目录则continue
                    continue;
                }

                if(p_dir2->d_type == DT_DIR)//目录名
                {
                    //不考虑子目录下让包含目录的情况
                }
                else//文件名
                {
                    reals.file_nums++;
                }
            }
            closedir(dir2);
        }
        else//文件名
        {
            reals.file_nums++;
        }
    }
    
    ESP_LOGW(TAG, "[filename_list_get] path : %s,  file_nums : %lu",path, reals.file_nums);

    closedir(dir);
    
//  historic_data_buf_clr();        // 释放目录与文件占用的动态内存
    if(NULL != fsys_file_list)
    {
        free(fsys_file_list);
    }   
    
    fsys_file_list = iot_calloc(reals.file_nums*sizeof(name_list_t2));//FILE_DIR_LEN

    if(fsys_file_list == NULL) 
    {
        ESP_LOGE(TAG, "[filename_list_get] heap_caps_malloc error");
        return -1;
    }   
    
    ///step 2:基于文件数量开辟内存
    dir = opendir(path);
    if(dir == NULL) 
    {
        ESP_LOGE(TAG, "[filename_list_get] opendir error(%s)",path);
        return -1;
    }

    while((p_dir = readdir(dir)) != NULL)//遍历指定目录路径下的所有文件，不包含子目录的子文件
    {
        if(strcmp(p_dir->d_name,".")==0 || strcmp(p_dir->d_name,"..")==0) 
        { // 是当前目录或父目录则continue
            continue;
        }

        if(p_dir->d_type == DT_DIR)//目录名
        {
            memcpy(folder, p_dir->d_name, sizeof(folder));
            sprintf(path2, "%s/%s", RECORD_ROOT_PATH, folder);

            dir2 = opendir(path2);
            if(dir2 == NULL) 
            {
                ESP_LOGE(TAG, "[filename_list_get] opendir2 error(%s)",path2);
                continue;
            }
 
            while((p_dir2 = readdir(dir2)) != NULL)//遍历指定目录路径下的所有文件，不包含子目录的子文件
            {  
                if(strcmp(p_dir2->d_name,".")==0 || strcmp(p_dir2->d_name,"..")==0) 
                { // 是当前目录或父目录则continue
                    continue;
                }

                if(p_dir2->d_type == DT_DIR)//目录名
                {
                    //不考虑子目录下让包含目录的情况
                }
                else//文件名
                {
                    if(file_nums < reals.file_nums)
                    {
                        strncpy(&fsys_file_list[file_nums].name[0], p_dir->d_name,1);
                        strncpy(&fsys_file_list[file_nums].name[1], p_dir2->d_name,9);
                        
                        sscanf(&fsys_file_list[file_nums].name[0],"%hu",&fsys_file_list[file_nums].filename_value);//name ->filename_value
                    }
                    file_nums++;
                }
            }
            closedir(dir2);
        }
        else//文件名
        {
            if(file_nums < reals.file_nums)
            {
                strncpy(&fsys_file_list[file_nums].name[1], p_dir->d_name,9);
                
                sscanf(&fsys_file_list[file_nums].name[0],"%hu",&fsys_file_list[file_nums].filename_value);//name ->filename_value
            }
            file_nums++;
        }
    }

    closedir(dir);
    
    return 0;
}

/*------------------------------------------------------------------------------
 Function: file_list_clear
 -----------------------------------------------------------------------------*/
/**
  * @brief      初始化，删除spi flash文件系统中指定类型（两个字节）文件（包括一级子目录内文件）
  * @param[in]  target_file_type (null表示全部删除)  
  * @param[out] None
  * @return     int
  */
int file_list_clear(char *target_file_type)
{
	char path[30] = {0};
    char path2[30] = {0};
    char path3[30] = {0};
    char folder[10] = {0};
    char fname[10] = {0};
    
    DIR *dir;
    DIR *dir2;
    struct dirent *p_dir;
    struct dirent *p_dir2;

	sprintf(path, "%s", RECORD_ROOT_PATH);

	dir = opendir(path);
	if(dir == NULL) 
    {
        ESP_LOGE(TAG, "[file_list_clear] opendir error");
        return -1;
    }

	while((p_dir = readdir(dir)) != NULL)//遍历指定目录路径下的所有文件，包含子目录的子文件
    {
        if(strcmp(p_dir->d_name,".")==0 || strcmp(p_dir->d_name,"..")==0) 
		{ // 是当前目录或父目录则continue
			continue;
		}

		if(p_dir->d_type == DT_DIR)//目录名
		{
		    memcpy(folder, p_dir->d_name, sizeof(folder));
            sprintf(path2, "%s/%s", RECORD_ROOT_PATH, folder);
            dir2 = opendir(path2);
        	if(dir2 == NULL) 
            {
                ESP_LOGE(TAG, "[file_list_clear] opendir2 error");
                continue;
            }
        	while((p_dir2 = readdir(dir2)) != NULL)//遍历指定目录路径下的所有文件，不包含子目录的子文件
            {                
                if(strcmp(p_dir2->d_name,".")==0 || strcmp(p_dir2->d_name,"..")==0) 
        		{ // 是当前目录或父目录则continue
        			continue;
        		}

        		if(p_dir2->d_type == DT_DIR)//目录名
        		{
                    //不考虑子目录下让包含目录的情况
        		}
        		else//文件名
        		{
        		    memcpy(fname, p_dir2->d_name, sizeof(fname));
                    sprintf(path3, "%s/%s/%s", RECORD_ROOT_PATH, folder, fname);
                    if (target_file_type != NULL)
                    {
                        if (strncmp(fname, target_file_type, 2) == 0)
                        {
                            remove(path3);
                            ESP_LOGW(TAG, "[file_list_clear] delete : %s", path3);
                        }
                    }
                    else
                    {
                        remove(path3);
                        ESP_LOGW(TAG, "[file_list_clear] delete : %s", path3);
                    }
        		}
            }
            closedir(dir2);
		}
		else//文件名
		{
            memcpy(fname, p_dir->d_name, sizeof(fname));
            sprintf(path3, "%s/%s", RECORD_ROOT_PATH, fname);
            if (target_file_type != NULL)
            {
                #ifdef FILE_SYSTEM_DIRECTORY_ENABLE
                
                if (strncmp(fname, target_file_type, 2) == 0)
                {
                    remove(path3);
                    ESP_LOGW(TAG, "[file_list_clear] delete : %s", path3);
                }
                
                #else
                
                if (strncmp(&fname[1], target_file_type, 2) == 0)
                {
                    remove(path3);
                    ESP_LOGW(TAG, "[file_list_clear] delete : %s", path3);
                }

                #endif
            }
            else
            {
                remove(path3);
                ESP_LOGW(TAG, "[file_list_clear] delete : %s", path3);
            }
		}
    }

	closedir(dir);
    
	return 0;
}

/*------------------------------------------------------------------------------
 Function: CheckHisotryLogResize
 -----------------------------------------------------------------------------*/
/**
  * @brief      检查当前文件长度，使其不超过HISTORY_LOG_RECORD_MAX_NUM条
  * @param[in]  const char* filename  
  * @param[out] None
  * @return     void
  */
void CheckHisotryLogResize(const char* filename) 
{
    //最多存储HISTORY_LOG_RECORD_MAX_NUM条
    uint16_t max_size = HISTORY_LOG_RECORD_MAX_NUM * sizeof(LOG_FAULT_STRUCT);
    uint8_t *buffer = NULL;
    
    // 使用open以读写模式打开文件，如果文件不存在则退出
    int file = open(filename, O_RDWR);
    if (file < 0) {
        return;
    }

    // 移动到文件末尾
    off_t fileSize = lseek(file, 0, SEEK_END); // 获取当前文件大小

    if (fileSize > max_size) {

        ESP_LOGE(TAG, "[CheckHisotryLogResize] fileSize > max_size");
        // 如果文件大于HISTORY_LOG_RECORD_MAX_NUM条，读取最后HISTORY_LOG_RECORD_MAX_NUM条
        lseek(file, -max_size, SEEK_END);
        
        buffer = iot_calloc(max_size);
    
        read(file, buffer, max_size);

        // 关闭并删除原文件，然后重新创建
        close(file);
        remove(filename);

        file = open(filename, O_RDWR | O_CREAT); // 以读写模式重新打开文件，创建新文件
        if (file < 0) {
            goto exit;
        }

        // 将读取的1KB数据写入新文件
        write(file, buffer, max_size);
    }

    // 关闭文件
    close(file);
    
    exit:
        
    if(buffer != NULL)    
    {
        free(buffer);
    }
}

static bool array_is_empty(uint8_t *array, uint16_t size)
{
    for (uint16_t i = 0; i < size; i++)
    {
        if (array[i] != 0)
        {
            return false;
        }
    }

    return true;
}

/*------------------------------------------------------------------------------
Function: clear_log_file
-----------------------------------------------------------------------------*/
/**
* @brief	  清除历史记录
* @param[in]  void	
* @param[out] None
* @return	  static void
*/
int clear_log_file(void)
{
    char path[50] = {0};
    // char fname[30] = {0};
    int fd;

    IotSetData.dev_info_t.historyRecSaveCount= 0;//log记录条数 
    IotSetData.dev_info_t.historyAddrIndex= 0;//log记录条数
    reals.SetDataWrFlag.sBit.SetDataUpdate_historycnt =1;   

    /*多逆变并机时单机分别检查*/
    if (1 < (DEV_MAIN_NODE_MAX*INV_MAX_NUM))
    {
    	for(uint8_t i = 1 ; i <= DEV_MAIN_NODE_MAX ; i++)
    	{
    		sprintf(path, "%s/%s%s%d_%02d", RECORD_ROOT_PATH, LOG_RECORD_FOLDER_LEVEL1, INV_RECORD_FILE_MARK, i, 0); 

    		/* 只读格式打开文件 */
    		fd = open(path, O_RDONLY);
    		if(fd < 0)
    		{
    			ESP_LOGE(TAG, "[log_file] open error, %s", path);
    			continue;
    		}

    		close(fd);
    		remove(path);
    		ESP_LOGW(TAG, "clear %s log_history ok", path);
    	}
    }
    
    /*清除汇总文件*/
	sprintf(path, "%s/%s%s%d_%02d", RECORD_ROOT_PATH, LOG_RECORD_FOLDER_LEVEL1, INV_RECORD_FILE_MARK, 0, 0); 
    
	/* 只读格式打开文件 */
	fd = open(path, O_RDONLY);
	if(fd < 0)
	{
		ESP_LOGE(TAG, "[log_file] open error, %s", path);
		return -1;
	}

	close(fd);
	remove(path);
	ESP_LOGW(TAG, "clear %s log_history ok", path);

    return 0;
}

/**
  * @brief 将 rtc_time_t 转为 time_t（UTC）
  * @param[in] rtc 指向 rtc_time_t
  * @return 转换后的 time_t（错误返回 (time_t)-1）
  * @note 使用 mktime 会依据本地时区；若要按 UTC 转换请使用 timegm（若可用）或自行调整时区。
  */
time_t rtc_time_to_time_t(const rtc_time_t *rtc)
{
    if (rtc == NULL) return (time_t)-1;

    /* 简单范围校验 */
    if (rtc->mon < 1 || rtc->mon > 12) return (time_t)-1;
    if (rtc->day < 1 || rtc->day > 31) return (time_t)-1;
    if (rtc->hour > 23) return (time_t)-1;
    if (rtc->min > 59) return (time_t)-1;
    if (rtc->sec > 60) return (time_t)-1; /* leap second tolerance */

    struct tm tm = {0};
    /* tm_year 是自 1900 年起的年数，rtc->year 基于 2000 */
    tm.tm_year = (int)rtc->year + 100; /* 2000 -> 100 */
    tm.tm_mon  = (int)rtc->mon - 1;
    tm.tm_mday = (int)rtc->day;
    tm.tm_hour = (int)rtc->hour;
    tm.tm_min  = (int)rtc->min;
    tm.tm_sec  = (int)rtc->sec;
    tm.tm_isdst = -1;
    time_t t = mktime(&tm);
    return t;
}


#define DATA_INIT_FROM_LITTLEFS_DEBUG_LOG

/*------------------------------------------------------------------------------
 Function: reverse_struct_array
 -----------------------------------------------------------------------------*/
/**
  * @brief      将结构体数组倒序
  * @param[in]  void *array       
                size_t elem_size  
                size_t count      
  * @param[out] None
  * @return     void
  */
static void reverse_struct_array(void *array, size_t elem_size, size_t count) {
    char *arr = (char *)array;
    for (size_t i = 0; i < count / 2; ++i) {
        // 交换 arr[i] 和 arr[count-1-i]
        for (size_t j = 0; j < elem_size; ++j) {
            char tmp = arr[i * elem_size + j];
            arr[i * elem_size + j] = arr[(count - 1 - i) * elem_size + j];
            arr[(count - 1 - i) * elem_size + j] = tmp;
        }
    }
}

/*------------------------------------------------------------------------------
 Function: GetHistoryLogFromLittlefs
 -----------------------------------------------------------------------------*/
/**
  * @brief      modbus3000历史记录读取
  * @param[in]  uint16_t history_page  
  * @param[out] None
  * @return     int
  */
int GetHistoryLogFromLittlefs(uint16_t history_page, uint8_t slaveaddress)
{
    char path[30] = {0};
    int fd;
    uint16_t page_size = HISTORY_PAGE_LOG_MAX_NUM * sizeof(LOG_FAULT_STRUCT);//每页最大字节数
    uint16_t max_size = HISTORY_LOG_RECORD_MAX_NUM * sizeof(LOG_FAULT_STRUCT);//最多存储100条
    int pages;//总页数
    uint16_t file_bia;//文件偏移/记录条数
    uint16_t file_pos;//文件偏移/size
    uint16_t read_size;//读取长度
    uint16_t read_bia = IotSetData.dev_info_t.historyAddrIndex;
    
    uint16_t index = 0;
    if ( slaveaddress == 0 ) {
        index = (DEV_MAIN_NODE_MAX*INV_MAX_NUM);              /*汇总*/
    } else {
        index = slaveaddress - 1;    /*单机*/
    }
    if(DEV_MAIN_NODE_MAX*INV_MAX_NUM == 1) // 不并机时，汇总和单机都读取汇总文件
    {
        slaveaddress = 0;
    }

    // 获取文件操作互斥锁，保护整个操作过程
    if (xInvLogFileProcessSemaphore && xSemaphoreTake(xInvLogFileProcessSemaphore, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "Failed to take file process semaphore for GetHistoryLogFromLittlefs.");
        return 0;
    }

    top_modbus_rd.Inv[index].mod_reg03000_Inv_history.current_page_seq = history_page;

    sprintf(path, "%s/%s%s%d_%02d", RECORD_ROOT_PATH, LOG_RECORD_FOLDER_LEVEL1, INV_RECORD_FILE_MARK, slaveaddress, 0);
    
    /* 只读格式打开文件 */
    fd = open(path, O_RDONLY);
    if(fd < 0)
    {
        ESP_LOGE(TAG, "[GetIotHistoryLogFromLittlefs] open error(%s)",path);
        memset((uint8_t *)&top_modbus_rd.Inv[index].mod_reg03000_Inv_history, 0, sizeof(top_modbus_rd.Inv[index].mod_reg03000_Inv_history));
        // 释放文件操作互斥锁
        xSemaphoreGive(xInvLogFileProcessSemaphore);
        return 0;
    }

    /*获取文件大小*/
    off_t fileSize = lseek(fd, 0, SEEK_END); 

    /*计算文件页大小*/
    if ( fileSize % page_size != 0 ) {
        pages = (uint8_t)(fileSize/page_size); 
    } else {
        pages = ((uint8_t)(fileSize/page_size) > 0) ? ((uint8_t)(fileSize/page_size) - 1) : 0;
    }
    
    if (pages < history_page)
    {
        /*越界访问，访问页不存在*/
        memset((uint8_t *)&top_modbus_rd.Inv[index].mod_reg03000_Inv_history, 0, sizeof(top_modbus_rd.Inv[index].mod_reg03000_Inv_history));
        ESP_LOGE(TAG, "[GetIotHistoryLogFromLittlefs] history_page error: %d, [0~%d]", history_page, pages);
        pages = -1;
        goto end;
    }

    /*尚未存储满HISTORY_LOG_RECORD_MAX_NUM条*/
    if (fileSize < max_size)
    {
        if (pages == history_page)
        {
            /* 移动文件指针到设置的数据位置 */
            file_pos = fileSize;
            
            /* 读数据到缓存 */
            read_size = fileSize - pages*page_size;
        }
        else
        {
            /* 移动文件指针到设置的数据位置 */
            file_pos = (history_page + 1) * page_size;
            
            /* 读数据到缓存 */
            read_size = page_size;
        }
        
        lseek(fd, -file_pos, SEEK_END);

        if(read(fd, &top_modbus_rd.Inv[index].mod_reg03000_Inv_history.log_data, read_size) != read_size) 
        {
            ESP_LOGE(TAG, "[GetIotHistoryLogFromLittlefs] read error(%s)",path);
            pages = -1;
            goto end;
        }
    }
    else
    {
        /*需要获取的历史纪录从当前位置偏移条数*/
        file_bia = (history_page + 1)*HISTORY_PAGE_LOG_MAX_NUM;

        if (file_bia <= read_bia)
        {
            /* 移动文件指针到设置的数据位置 */
            file_pos = (read_bia - file_bia) * sizeof(LOG_FAULT_STRUCT);
            lseek(fd, file_pos, SEEK_SET);
            
            /* 读数据到缓存 */
            read_size = page_size;
            if(read(fd, &top_modbus_rd.Inv[index].mod_reg03000_Inv_history.log_data, read_size) != read_size) 
            {
                ESP_LOGE(TAG, "[GetIotHistoryLogFromLittlefs] read error(%s)",path);
                pages = -1;
                goto end;
            }
        }
        else if (file_bia >= (read_bia + HISTORY_PAGE_LOG_MAX_NUM))
        {
            /* 移动文件指针到设置的数据位置 */
            file_pos = (file_bia - read_bia) * sizeof(LOG_FAULT_STRUCT);
            lseek(fd, -file_pos, SEEK_END);
            
            /* 读数据到缓存 */
            read_size = page_size;
            if(read(fd, &top_modbus_rd.Inv[index].mod_reg03000_Inv_history.log_data, read_size) != read_size) 
            {
                ESP_LOGE(TAG, "[GetIotHistoryLogFromLittlefs] read error(%s)",path);
                pages = -1;
                goto end;
            }
        }
        /*需要拼接*/
        else
        {
            /* 移动文件指针到设置的数据位置 */
            file_pos = (file_bia - read_bia) * sizeof(LOG_FAULT_STRUCT);
            lseek(fd, -file_pos, SEEK_END);
            
            /* 读数据到缓存 */
            read_size = file_pos;
            if(read(fd, &top_modbus_rd.Inv[index].mod_reg03000_Inv_history.log_data[0], read_size) != read_size) 
            {
                ESP_LOGE(TAG, "[GetIotHistoryLogFromLittlefs] read error(%s)",path);
                pages = -1;
                goto end;
            }
            
            /* 移动文件指针到设置的数据位置 */
            file_pos = 0;
            lseek(fd, file_pos, SEEK_SET);
            
            /* 读数据到缓存 */
            read_size = ((read_bia + HISTORY_PAGE_LOG_MAX_NUM) - file_bia) * sizeof(LOG_FAULT_STRUCT);
            if(read(fd, &top_modbus_rd.Inv[index].mod_reg03000_Inv_history.log_data[file_bia - read_bia], read_size) != read_size) 
            {
                ESP_LOGE(TAG, "[GetIotHistoryLogFromLittlefs] read error(%s)",path);
                pages = -1;
                goto end;
            }
        }
    }
    end:


    close(fd);
    
    // 释放文件操作互斥锁
    xSemaphoreGive(xInvLogFileProcessSemaphore);

#if 0
     /*历史记录倒序排列*/
     if ( pages >= 0 ) {
         uint8_t log_num = fileSize / sizeof(LOG_FAULT_STRUCT);
         log_num = (log_num > 5) ? 5 : log_num;
         if ( log_num > 0 ) {
             reverse_struct_array(&top_modbus_rd.Inv[index].mod_reg03000_Inv_history.log_data[0], sizeof(LOG_FAULT_STRUCT), log_num);
         }
     }
#endif

    return (pages + 1);

}

/*------------------------------------------------------------------------------
 Function: history_queue_init
 -----------------------------------------------------------------------------*/
/**
  * @brief      历史记录存储队列初始化
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void history_queue_init(void)
{
    // 日志文件操作互斥信号量,防止多任务同时访问
    if (xIoTEventLogFileProcessSemaphore == NULL) { 
        xIoTEventLogFileProcessSemaphore = xSemaphoreCreateMutex();
        if (xIoTEventLogFileProcessSemaphore == NULL) {
            ESP_LOGE(TAG, "xIoTEventLogFileProcessSemaphore Create failed");
            abort();
        }
    }
    if (xInvLogFileProcessSemaphore == NULL) { 
        xInvLogFileProcessSemaphore = xSemaphoreCreateMutex();
        if (xInvLogFileProcessSemaphore == NULL) {
            ESP_LOGE(TAG, "xInvLogFileProcessSemaphore Create failed");
            abort();
        }
    }

    /* 创建log存储消息队列 */
    xQueue_Log_record = xQueueCreate(32, sizeof(LOG_FAULT_STRUCT_queue_struct));
    if (!xQueue_Log_record) {
        ESP_LOGE (TAG, "xQueue_Log_record queue create failed");
		abort();
    }
    xQueue_iot_record_event = xQueueCreate(16, sizeof(LOG_EventHistoryData_Struct));
    if (!xQueue_iot_record_event) {
        ESP_LOGE (TAG, "xQueue_iot_record_event queue create failed");
		abort();
    }

    reals.EventFlag.sBit.turn_on = 1;
}

/*------------------------------------------------------------------------------
 Function: Event_Pop
 -----------------------------------------------------------------------------*/
/**
  * @brief      从已保存到缓存的本地事件队列写入到flash
  * @param[in]  void  
  * @param[out] None
  * @return     uint8_t
  */
uint8_t Event_Pop(void)
{
	LOG_EventHistoryData_Struct queue_msg =  {NULL};
	char fname[30] = {0};
	uint32_t file_bias =0;
	uint8_t rtn=0xFF;

    // 获取文件操作互斥锁，保护整个操作过程
    // 避免获取锁失败导致本次拉取到的记录丢失
    if (xIoTEventLogFileProcessSemaphore && xSemaphoreTake(xIoTEventLogFileProcessSemaphore, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "Save_Task failed to take file process semaphore. Records will be discarded.");
        return rtn;
    }

	if(xQueue_iot_record_event && xQueueReceive(xQueue_iot_record_event, &queue_msg, 0) == pdTRUE)
	{
	    /*文件名*/
		LOG_FILE_PATH_IOT(fname, 1, EVENT_IOT_PROTOCOL);
        
		file_bias = (uint16_t)IotSetData.dev_info_t.Event_AddrIndex;
        
        /*写入文件，若不存在则创建*/
		if (0 == historic_data_write(fname, (uint8_t *)queue_msg.pdata, file_bias, sizeof(EventHistoryData_Struct))) 
		{
            ESP_LOGI(TAG, "File written ok (%s), bias: %ld",fname, IotSetData.dev_info_t.Event_AddrIndex);
            rtn = 0;

            // 偏移地址
            IotSetData.dev_info_t.Event_AddrIndex++;
            if (IotSetData.dev_info_t.Event_AddrIndex >= EVENT_LOG_RECORD_MAX_NUM) {
                IotSetData.dev_info_t.Event_AddrIndex = 0;
                IotSetData.dev_info_t.Event_SaveCount = EVENT_LOG_RECORD_MAX_NUM;
            }

            // 存储条数
            if(IotSetData.dev_info_t.Event_SaveCount != EVENT_LOG_RECORD_MAX_NUM) {
                IotSetData.dev_info_t.Event_SaveCount = IotSetData.dev_info_t.Event_AddrIndex;
            }
            
            reals.SetDataWrFlag.sBit.SetDataUpdate_Event_cnt =1;				 
		}
		else
		{
            ESP_LOGE(TAG, "File written fail:Event_Pop (%s), exit", fname );
		}
	
		if (queue_msg.pdata) 
		{
			free(queue_msg.pdata); 
			queue_msg.pdata = NULL; 
		}
	}
    
    // 释放文件操作互斥锁
    xSemaphoreGive(xIoTEventLogFileProcessSemaphore);
    
    return rtn;
}


/*------------------------------------------------------------------------------
 Function: Event_Push
 -----------------------------------------------------------------------------*/
/**
  * @brief      事件记录存储到队列缓存
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void Event_Push(void)
{
	LOG_EventHistoryData_Struct queue_msg =  {NULL};

    queue_msg.pdata = (EventHistoryData_Struct *)iot_calloc(sizeof(EventHistoryData_Struct));

	if (!queue_msg.pdata) 
	{
		ESP_LOGE(TAG, "iot_Event_Push calloc failed");
	} 
	else 
	{
		memcpy(queue_msg.pdata, &HistoryData_event, sizeof(EventHistoryData_Struct));
        
		/*消息保存到队列*/
		if (xQueueSendToBack((QueueHandle_t)xQueue_iot_record_event, &queue_msg, 0) != pdPASS) 
		{
			ESP_LOGE(TAG, "iot_Event_Push push queue failed");
			free(queue_msg.pdata);
			queue_msg.pdata = NULL;
		}
	}
}

/*------------------------------------------------------------------------------
 Function: SaveOneEvent
 -----------------------------------------------------------------------------*/
/**
  * @brief      生成事件记录
  * @param[in]  EventHistoryData_Struct *HistoryData  
  * @param[out] None
  * @return     void
  */
void SaveOneEvent(EventHistoryData_Struct *HistoryData)
{
	memcpy(&HistoryData_event, HistoryData, sizeof(EventHistoryData_Struct));
    
    /*发送至队列，等待写入文件系统*/
	Event_Push();
    ESP_LOGI(TAG, "event_check : ver_protocol(%d), WarnCode(%d), FaultCode(%d)", HistoryData->ver_protocol, HistoryData->WarnCode, HistoryData->FaultCode);
}

/**
  * @brief    计算自 since_timestamp 之后的新事件记录数
  * @param[in] since_timestamp  时间戳基准（0 表示统计全部）
  * @return   新记录数（uint16_t）
  *
  * 说明：
  * - 逻辑索引按从0到Event_SaveCount-1排列（0为最早的逻辑记录，当文件未环满时物理索引==逻辑索引）；
  * - 若读取关键位置（最旧/最新）失败，函数返回0表示无法确定或无新记录；
  */
static uint16_t IoT_Event_Log_Count_New_Records(time_t since_timestamp)
{
    // 1. 基本参数和路径检查
    char path[30] = {0};
	sprintf(path, "%s/%s%s%02d_c%02d", FS_BASE_PATH, LOG_RECORD_FOLDER_LEVEL1, IOT_RECORD_FILE_MARK, 1, EVENT_IOT_PROTOCOL);

    if (IotSetData.dev_info_t.Event_SaveCount == 0) {
        return 0; // 模块未初始化或文件为空，没有新记录
    }

    // 2. 快速路径检查：处理全量同步的情况
    if (since_timestamp == 0) {
        ESP_LOGI(TAG, "since_timestamp is 0, counting all records.");
        return IotSetData.dev_info_t.Event_SaveCount;
    }

    // 3. 快速路径检查：与最旧和最新的记录比较
    uint16_t oldest_record_p_idx; // 最旧记录的物理索引
    if (IotSetData.dev_info_t.Event_SaveCount < EVENT_LOG_RECORD_MAX_NUM) {
        // 文件未满，最旧的记录在索引0
        oldest_record_p_idx = 0;
    } else {
        // 文件已满，最旧的记录在当前的写指针位置
        oldest_record_p_idx = IotSetData.dev_info_t.Event_AddrIndex;
    }

    EventHistoryData_Struct temp_record;
    uint32_t record_offset = oldest_record_p_idx * IOT_EVENT_FUNC_04_TYPE_LEN;
    if (fs_file_read_at(path, record_offset, (uint8_t*)&temp_record, IOT_EVENT_FUNC_04_TYPE_LEN) == IOT_EVENT_FUNC_04_TYPE_LEN) {
        // 如果 since_timestamp 早于文件中最旧的记录，则所有记录都是新的
        if (since_timestamp < temp_record.time_s) {
            ESP_LOGI(TAG, "since_timestamp is older than the oldest record, counting all records.");
            return IotSetData.dev_info_t.Event_SaveCount;
        }
    } else {
        ESP_LOGE(TAG, "Failed to read oldest record at index %u for pre-check.", oldest_record_p_idx);
        return 0; // 关键记录读取失败，无法继续
    }

    // 在逻辑上，最新的记录是第 (current_records - 1) 条。
    uint16_t newest_record_l_idx = IotSetData.dev_info_t.Event_SaveCount - 1;
    // 其物理索引 = (最旧记录的物理索引 + 最新记录的逻辑索引) % 数组总容量
    uint16_t newest_record_p_idx = (oldest_record_p_idx + newest_record_l_idx) % EVENT_LOG_RECORD_MAX_NUM;
    
    record_offset = newest_record_p_idx * IOT_EVENT_FUNC_04_TYPE_LEN;
    if (fs_file_read_at(path, record_offset, (uint8_t*)&temp_record, IOT_EVENT_FUNC_04_TYPE_LEN) == IOT_EVENT_FUNC_04_TYPE_LEN) {
        // 如果 since_timestamp 晚于文件中最新的记录，则所有记录都是旧的
        if (since_timestamp >= temp_record.time_s) {
            ESP_LOGW(TAG, "since_timestamp is newer than or equal to the newest record, no new records.");
            return 0;
        }
    } else {
        ESP_LOGE(TAG, "Failed to read newest record at index %u for pre-check.", newest_record_p_idx);
        return 0; // 关键记录读取失败，无法继续
    }

    // --- 4. 二分查找：定位第一个新记录 ---
    // 在逻辑索引 [0, current_records - 1] 上进行二分查找。
    int low = 0, high = IotSetData.dev_info_t.Event_SaveCount - 1;
    int first_new_l_idx = -1; // 第一个新记录的逻辑索引

    while (low <= high) {
        int mid_l_idx = low + (high - low) / 2; // 中间点的逻辑索引

        // 将逻辑索引转换为物理文件索引
        // 当前物理索引 = (最旧记录的物理索引 + 当前逻辑索引) % 数组总容量
        uint16_t mid_p_idx = (oldest_record_p_idx + mid_l_idx) % EVENT_LOG_RECORD_MAX_NUM;
        
        record_offset = mid_p_idx * IOT_EVENT_FUNC_04_TYPE_LEN;
        if (fs_file_read_at(path, record_offset, (uint8_t*)&temp_record, IOT_EVENT_FUNC_04_TYPE_LEN) != IOT_EVENT_FUNC_04_TYPE_LEN) {
            ESP_LOGE(TAG, "Binary search failed to read record at physical index %u", mid_p_idx);
            return 0; // 查找失败
        }

        if (temp_record.time_s > since_timestamp) {
            // 这是一个新记录。
            // 找到了一个潜在的边界，记录它，并尝试在更早的部分（左半区）寻找更早的新记录。
            first_new_l_idx = mid_l_idx;
            high = mid_l_idx - 1;
        } else {
            // 这是一个旧记录。
            // 在更晚的部分（右半区）寻找新记录。
            low = mid_l_idx + 1;
        }
    }

    // 5. 计算并返回结果
    if (first_new_l_idx != -1) {
        // 新记录的数量 = 总记录数 - 第一个新记录的逻辑索引
        return IotSetData.dev_info_t.Event_SaveCount - first_new_l_idx;
    }

    return 0; // 没有找到任何新记录
}

/**
 * @brief      读取增量日志数据到动态分配的缓冲区。
 * @details    此函数是实现日志增量同步的核心接口。它会：
 *             1. 调用辅助函数  计算出新记录的数量。
 *             2. 根据新记录数量计算总大小（包含文件头），并分配相应大小的内存。
 *             3. 从文件开头读取文件头，然后从文件中读取所有新记录，
 *                并将它们按时间顺序（旧->新）存入缓冲区。
 *             4. 通过输出参数返回缓冲区地址和总大小。
 *
 * @param[in]  since_timestamp  一个UNIX时间戳，用于界定“新”记录。
 * @param[out] out_buffer       一个指向 `uint8_t*` 的指针。如果函数成功，
 *                              它将被设置为新分配的缓冲区的地址。
 * @param[out] out_size         一个指向 `uint32_t` 的指针。如果函数成功，
 *                              它将被设置为缓冲区的总大小（字节）。
 *
 * @return     int
 *             - 0: 成功 (即使没有新记录也返回成功)。
 *             - -1: 失败（如内存分配失败、文件读取失败等）。
 *
 * @note       **重要**: 调用者在处理完 `out_buffer` 中的数据后，
 *             **必须**负责调用 `free(out_buffer)` 来释放内存。
 */
int IoT_Event_Log_Read_Incremental(time_t since_timestamp, uint8_t **out_buffer, uint32_t *out_size)
{
    // 1. 参数校验
    if (out_buffer == NULL || out_size == NULL) {
        return -1;
    }
    
    // 获取文件操作互斥锁，保护整个读操作过程
    if (xIoTEventLogFileProcessSemaphore && xSemaphoreTake(xIoTEventLogFileProcessSemaphore, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "Failed to take file process semaphore for reading.");
        return -1;
    }

    int ret = 0;
    uint8_t *buffer = NULL;
    
    *out_buffer = NULL;
    *out_size = 0;

    // 2. 调用辅助函数，获取新记录的数量
    uint16_t new_records_count = IoT_Event_Log_Count_New_Records(since_timestamp);

    // 如果没有新记录，则无需分配内存和读取，直接返回成功
    if (new_records_count == 0) {
        ESP_LOGW(TAG, "No new records found since timestamp %lld.", since_timestamp);
        goto exit_point; 
    }

    ESP_LOGI(TAG, "Found %u new records since timestamp %lld. Preparing to read.", new_records_count, since_timestamp);

    // 3. 计算总大小并分配内存
    uint32_t total_size = new_records_count * IOT_EVENT_FUNC_04_TYPE_LEN;
    buffer = (uint8_t *)iot_calloc(total_size);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %lu bytes for incremental log.", total_size);
        ret = -1;
        goto exit_point;
    }

    // 4. 读取所有新记录到缓冲区
    char path[30] = {0};
	sprintf(path, "%s/%s%s%02d_c%02d", FS_BASE_PATH, LOG_RECORD_FOLDER_LEVEL1, IOT_RECORD_FILE_MARK, 1, EVENT_IOT_PROTOCOL);
    //    首先，找到文件中第一个新记录的物理位置
    uint16_t oldest_record_p_idx; // 最旧记录的物理索引
    if (IotSetData.dev_info_t.Event_SaveCount < EVENT_LOG_RECORD_MAX_NUM) {
        // 文件未满，最旧的记录在索引0
        oldest_record_p_idx = 0;
    } else {
        // 文件已满，最旧的记录在当前的写指针位置
        oldest_record_p_idx = IotSetData.dev_info_t.Event_AddrIndex;
    }
    // 第一个新记录的逻辑索引 = 总记录数 - 新记录数
    uint16_t first_new_l_idx = IotSetData.dev_info_t.Event_SaveCount - new_records_count;
    // 第一个新记录的物理索引 = (最旧记录的物理索引 + 当前逻辑索引) % 数组总容量
    uint16_t first_new_p_idx = (oldest_record_p_idx + first_new_l_idx) % EVENT_LOG_RECORD_MAX_NUM;

    // 分一或两块，将所有新记录顺序读入缓冲区
    uint8_t *record_write_ptr = buffer;
    uint16_t space_to_end = EVENT_LOG_RECORD_MAX_NUM - first_new_p_idx;

    if (new_records_count <= space_to_end) {
        // 情况A: 所有新记录都在一个连续的块中
        uint32_t read_offset = first_new_p_idx * IOT_EVENT_FUNC_04_TYPE_LEN;
        if (fs_file_read_at(path, read_offset, record_write_ptr, new_records_count * IOT_EVENT_FUNC_04_TYPE_LEN) != new_records_count * IOT_EVENT_FUNC_04_TYPE_LEN) {
            ESP_LOGE(TAG, "Failed to read continuous block of new records.");
            ret = -1;
            goto exit_point;
        }
    } else {
        // 情况B: 新记录跨越了物理文件的末尾，需要分两次读取
        uint16_t cnt1 = space_to_end;
        uint16_t cnt2 = new_records_count - cnt1;

        // 读取第一部分 (从 first_new_p_idx 到文件末尾)
        uint32_t read_offset1 = first_new_p_idx * IOT_EVENT_FUNC_04_TYPE_LEN;
        if (fs_file_read_at(path, read_offset1, record_write_ptr, cnt1 * IOT_EVENT_FUNC_04_TYPE_LEN) != cnt1 * IOT_EVENT_FUNC_04_TYPE_LEN) {
            ESP_LOGE(TAG, "Failed to read first part of wrapped records.");
            ret = -1;
            goto exit_point;
        }

        // 读取第二部分 (从文件开头到剩余记录结束)
        uint32_t read_offset2 = 0; // 从记录区开头读
        if (fs_file_read_at(path, read_offset2, record_write_ptr + cnt1 * IOT_EVENT_FUNC_04_TYPE_LEN, cnt2 * IOT_EVENT_FUNC_04_TYPE_LEN) != cnt2 * IOT_EVENT_FUNC_04_TYPE_LEN) {
            ESP_LOGE(TAG, "Failed to read second part of wrapped records.");
            ret = -1;
            goto exit_point;
        }
    }

    // 6. 成功，返回缓冲区地址和大小
    *out_buffer = buffer;
    *out_size = total_size;

exit_point:
    // 统一的出口：处理资源释放
    if (ret != 0 && buffer != NULL) {
        // 如果函数执行失败，且内存已分配，则释放内存
        free(buffer);
        *out_buffer = NULL; // 确保外部不会使用悬空指针
        *out_size = 0;
    }

    // 释放文件操作互斥锁
    xSemaphoreGive(xIoTEventLogFileProcessSemaphore);
    
    return ret;
}

/*------------------------------------------------------------------------------
 Function: Is_Event_Log_File
 -----------------------------------------------------------------------------*/
/**
  * @brief      检查是否为IOT事件
  * @param[in]  const char *fname  
  * @param[out] None
  * @return     int
  */
int Is_Event_Log_File(const char *fname)
{
    // 1. 参数校验
    if (fname == NULL) {
        return -1;
    }

    // 2. 根据传入的参数构建请求的文件路径
    //    fname[0] 是目录, &fname[1] 是文件名
    char request_path[40] = {0};
    char filedir = fname[0];
    char* filename = &fname[1];

    if ( (LOG_RECORD_FOLDER_LEVEL1)[0] != filedir ) {
        // 目录不匹配，不是目标文件
        // ESP_LOGE(TAG, "dir is error ：%c.", filedir);
        return -1;
    }
    
    // 使用 snprintf 安全地构建路径，防止溢出
    snprintf(request_path, sizeof(request_path), "%s/%s%s", FS_BASE_PATH, LOG_RECORD_FOLDER_LEVEL1, filename);

    // 3. 使用与日志模块内部完全相同的宏来生成标准文件名
    char local_log_path[40] = {0};
	sprintf(local_log_path, "%s/%s%s%02d_c%02d", FS_BASE_PATH, LOG_RECORD_FOLDER_LEVEL1, IOT_RECORD_FILE_MARK, 1, EVENT_IOT_PROTOCOL);

    // 4. 比较两个路径字符串
    if (strcmp(request_path, local_log_path) == 0) {
        // 两个字符串完全相同，确认是目标日志文件
        return 0;
    }

    // 字符串不匹配，不是目标文件
    // ESP_LOGE(TAG, "request_path is error ：%s.", request_path);
    return -1;
}

/**
 * @brief 生成当前日志文件对应的文件标识符。
 *
 * - out_buf[0] = 文件夹层级字符
 * - &out_buf[1] = 文件名字符串（不含路径分隔符）
 *
 * @param[out] out_buf  输出缓冲区，至少应能容纳 1 + 文件名长度
 * @param[in]  buf_len  out_buf 长度（字节）
 *
 * @return int
 *         - 0 : 成功（out_buf 被填充）
 *         - -1: 参数错误或缓冲区不足
 */
int Get_IoT_Event_Log_File_Id(char *out_buf, size_t buf_len)
{
    if (out_buf == NULL || buf_len < 2) {
        return -1;
    }

    char local_log_path[30] = {0};
	sprintf(local_log_path, "%s/%s%s%02d_c%02d", FS_BASE_PATH, LOG_RECORD_FOLDER_LEVEL1, IOT_RECORD_FILE_MARK, 1, EVENT_IOT_PROTOCOL);

    const char *folder_char = (LOG_RECORD_FOLDER_LEVEL1 && LOG_RECORD_FOLDER_LEVEL1[0]) ?
                                &LOG_RECORD_FOLDER_LEVEL1[0] : NULL;
    if (folder_char == NULL) {
        return -1;
    }

    // 提取文件名部分（最后一个 '/' 之后）
    const char *p = strrchr(local_log_path, '/');
    const char *filename = p ? p + 1 : local_log_path;
    size_t fnlen = strlen(filename);

    if (fnlen + 1 > buf_len) { // 1 字节 folder + fnlen
        return -1;
    }

    out_buf[0] = folder_char[0];
    memcpy(out_buf + 1, filename, fnlen); // 不包含终止符

    return 0;
}

/**
 * @brief 统计晚于指定时间戳的日志新记录数（含信号量保护）。
 *
 * @param[in] since_timestamp  自 Unix 纪元起的时间戳；传 0 表示统计全部记录。
 *
 * @return int
 *         - >=0 : 新记录数量
 *         - -1  : 参数错误或其它不可恢复错误
 *         - -2  : 获取文件处理信号量失败（记录将被丢弃）
 */
int Is_IoT_Event_Log_Count_New(time_t since_timestamp)
{
    uint16_t new_count = 0;

    /* 获取信号量保护 */
    if (xIoTEventLogFileProcessSemaphore && xSemaphoreTake(xIoTEventLogFileProcessSemaphore, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "failed to take file process semaphore. Records will be discarded.");
        return -2;
    }

    /* 统计晚于 since_timestamp 的新记录数量（内部处理文件读错时返回0） */
    new_count = IoT_Event_Log_Count_New_Records(since_timestamp);

    /* 释放信号量 */
    if (xIoTEventLogFileProcessSemaphore) {
        xSemaphoreGive(xIoTEventLogFileProcessSemaphore);
    }

    return (int)new_count;
}

/*------------------------------------------------------------------------------
 Function: event_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      处理IOT事件信息
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void event_check(void)
{
    EventHistoryData_Struct HistoryData = {0};
	static uint32_t pre_time = 0;
	uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    /*周期日志*/
	if((now_time - pre_time) >= H4_HOUR_INTERVAL) 
    {   
        reals.EventFlag.sBit.period = 1;
        ESP_LOGI(TAG, "IoT Time : [%u-%u-%u  %u:%u:%u] ",(uint16_t)reals.rtc_time.year+2000,(uint16_t)reals.rtc_time.mon,(uint16_t)reals.rtc_time.day,
             (uint16_t)reals.rtc_time.hour,(uint16_t)reals.rtc_time.min,(uint16_t)reals.rtc_time.sec);   
        pre_time = now_time;
    }

    /*存在事件时执行*/
    if ( reals.EventFlag.Byte2 )
    {
        HistoryData.Num_IOT = reals.online_Iot_num;
        HistoryData.Num_INV = reals.online_Inv_num;
        HistoryData.Num_PACK = reals.online_Pack_num;
        HistoryData.time_wifi_connect = (reals.wifi_connect_timestemp > 0) ? ((now_time - reals.wifi_connect_timestemp) / 1000 / 60) : 0;
        HistoryData.time_sub1g_connect = 0;//未更新
        HistoryData.time_s = reals.now;
        
        /*开机*/
        if (reals.EventFlag.sBit.turn_on == 1)
        {
            HistoryData.ver_protocol = EVENT_IOT_PROTOCOL;
            HistoryData.FaultCode = 0;
            HistoryData.FaultState = 0;
            HistoryData.WarnCode = CODE_ALARM_ON;
            HistoryData.WarnState = 1;
            SaveOneEvent(&HistoryData);
            reals.EventFlag.sBit.turn_on = 0;
        }
        
        /*关机*/
        if (reals.EventFlag.sBit.turn_off == 1)
        {
            HistoryData.ver_protocol = EVENT_IOT_PROTOCOL;
            HistoryData.FaultCode = 0;
            HistoryData.FaultState = 0;
            HistoryData.WarnCode = CODE_ALARM_OFF;
            HistoryData.WarnState = 1;
            SaveOneEvent(&HistoryData);
            reals.EventFlag.sBit.turn_off = 0;
        }
        
        /*周期*/
        if (reals.EventFlag.sBit.period == 1)
        {
            HistoryData.ver_protocol = EVENT_IOT_PROTOCOL;
            HistoryData.FaultCode = 0;
            HistoryData.FaultState = 0;
            HistoryData.WarnCode = CODE_ALARM_PERIOD_RECORD;
            HistoryData.WarnState = 1;
            SaveOneEvent(&HistoryData);
            reals.EventFlag.sBit.period = 0;
        }
    }


    /*从已保存到缓存的本地事件队列写入到flash*/
    Event_Pop();
}   


/*------------------------------------------------------------------------------
 Function: Logger_Pop
 -----------------------------------------------------------------------------*/
/**
  * @brief      从已保存到缓存的历史记录队列写入到flash
  * @param[in]  void  
  * @param[out] None
  * @return     uint8_t
  */
uint8_t Logger_Pop(void)// 
{
	LOG_FAULT_STRUCT_queue_struct queue_msg = {NULL};
	char fname[30] = {0};
	uint32_t file_bias =0;
	uint8_t rtn=0xFF;

    // 获取文件操作互斥锁，保护整个操作过程
    // 避免获取锁失败导致本次拉取到的记录丢失
    if (xInvLogFileProcessSemaphore && xSemaphoreTake(xInvLogFileProcessSemaphore, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "Save_Task failed to take file process semaphore. Records will be discarded.");
        return rtn;
    }

	if(xQueue_Log_record && xQueueReceive(xQueue_Log_record, &queue_msg, 0) == pdTRUE)
	{
	    /*文件名，当前为littlefs/L/inv0_0*/
		INV_FILE_PATH(fname,queue_msg.INV_NUM,0);
        
        /*检查记录长度*/
        //CheckHisotryLogResize(fname);

		file_bias = (uint16_t)IotSetData.dev_info_t.historyAddrIndex;

        /*写入文件，若不存在则创建*/
		if (0 == historic_data_write(fname, (uint8_t *)queue_msg.pdata, file_bias, sizeof(LOG_FAULT_STRUCT))) 
		{
            ESP_LOGI(TAG, "File written ok (%s), bias: %ld",fname, IotSetData.dev_info_t.historyAddrIndex);
            rtn = 0;

            // 偏移地址
            IotSetData.dev_info_t.historyAddrIndex++;
            if (IotSetData.dev_info_t.historyAddrIndex >= HISTORY_LOG_RECORD_MAX_NUM) {
                IotSetData.dev_info_t.historyAddrIndex = 0;
                IotSetData.dev_info_t.historyRecSaveCount = HISTORY_LOG_RECORD_MAX_NUM;
            }
            
            // 存储条数
            if(IotSetData.dev_info_t.historyRecSaveCount != HISTORY_LOG_RECORD_MAX_NUM) {
                IotSetData.dev_info_t.historyRecSaveCount = IotSetData.dev_info_t.historyAddrIndex;
            }
            
            reals.SetDataWrFlag.sBit.SetDataUpdate_historycnt =1;				 
		}
		else
		{
            ESP_LOGE(TAG, "File written fail (%s), exit", fname );
		}
		
	
		if (queue_msg.pdata) 
		{
			free(queue_msg.pdata); 
			queue_msg.pdata = NULL; 
	
		}
	
	}
    
    // 释放文件操作互斥锁
    xSemaphoreGive(xInvLogFileProcessSemaphore);
    
    return rtn;
}

/*------------------------------------------------------------------------------
 Function: Logger_Push
 -----------------------------------------------------------------------------*/
/**
  * @brief      实时告警记录存储到队列缓存
  * @param[in]  uint8_t node_id  
  * @param[out] None
  * @return     void
  */
void Logger_Push(uint8_t node_id)
{
	LOG_FAULT_STRUCT_queue_struct queue_msg =  {NULL};

	queue_msg.pdata = (LOG_FAULT_STRUCT *)iot_calloc(sizeof(LOG_FAULT_STRUCT));
	if (!queue_msg.pdata) 
	{
		ESP_LOGE(TAG, "xQueue_Log_record message and malloc failed");
	} 
	else 
	{
		memcpy(queue_msg.pdata, &reals.inv_log_fault_info, sizeof(reals.inv_log_fault_info));
        
        if ( node_id == (DEV_MAIN_NODE_MAX*INV_MAX_NUM) ) {
            queue_msg.INV_NUM = 0;              /*汇总*/
        } else {
		    queue_msg.INV_NUM = node_id + 1;    /*单机*/
        }
        
		// 消息保存到队列
		if (xQueueSendToBack((QueueHandle_t)xQueue_Log_record, &queue_msg, 0) != pdPASS) 
		{
			ESP_LOGE(TAG, "Logger_Push message push queue failed");
			free(queue_msg.pdata);
			queue_msg.pdata = NULL;
		}
	}
}

/*------------------------------------------------------------------------------
 Function: inv_SaveOneErrorcode
 -----------------------------------------------------------------------------*/
/**
  * @brief      生成告警/故障记录
  * @param[in]  uint8_t HappenType         
                uint8_t ErrorCode          
                uint16_t FaultInformation  
  * @param[out] None
  * @return     void
  */
void inv_SaveOneErrorcode(uint8_t HappenType,uint8_t ErrorCode,uint16_t FaultInformation,uint8_t index)
{
    /*生成告警/故障记录*/
	reals.inv_log_fault_info.LogFaultSeq.FaultState = HappenType;
	reals.inv_log_fault_info.LogFaultSeq.FaultSeq = ErrorCode;
	reals.inv_log_fault_info.LogFaultCode = FaultInformation;
	memcpy(&reals.inv_log_fault_info.LogTime, &reals.rtc_time, sizeof(reals.rtc_time));

    /*发送至队列，等待写入文件系统*/
	Logger_Push(index);
}

/**
  * @brief    计算自 since_timestamp 之后的新事件记录数
  * @param[in] since_timestamp  时间戳基准（0 表示统计全部）
  * @return   新记录数（uint16_t）
  *
  * 说明：
  * - 逻辑索引按从0到Event_SaveCount-1排列（0为最早的逻辑记录，当文件未环满时物理索引==逻辑索引）；
  * - 若读取关键位置（最旧/最新）失败，函数返回0表示无法确定或无新记录；
  */
static uint16_t Inv_Log_Count_New_Records(time_t since_timestamp)
{
    // 1. 基本参数和路径检查
    char path[30] = {0};
    sprintf(path, "%s/%s%s%d_%02d", FS_BASE_PATH, LOG_RECORD_FOLDER_LEVEL1, INV_RECORD_FILE_MARK, 0, 0);

    if (IotSetData.dev_info_t.historyRecSaveCount == 0) {
        return 0; // 模块未初始化或文件为空，没有新记录
    }

    // 2. 快速路径检查：处理全量同步的情况
    if (since_timestamp == 0) {
        ESP_LOGD(TAG, "since_timestamp is 0, counting all records.");
        return IotSetData.dev_info_t.historyRecSaveCount;
    }

    // 3. 快速路径检查：与最旧和最新的记录比较
    uint16_t oldest_record_p_idx; // 最旧记录的物理索引
    if (IotSetData.dev_info_t.historyRecSaveCount < HISTORY_LOG_RECORD_MAX_NUM) {
        // 文件未满，最旧的记录在索引0
        oldest_record_p_idx = 0;
    } else {
        // 文件已满，最旧的记录在当前的写指针位置
        oldest_record_p_idx = IotSetData.dev_info_t.historyAddrIndex;
    }

    LOG_FAULT_STRUCT temp_record;
    size_t Inv_Log_Max_Len = sizeof(LOG_FAULT_STRUCT);
    uint32_t record_offset = oldest_record_p_idx * Inv_Log_Max_Len;
    if (fs_file_read_at(path, record_offset, (uint8_t*)&temp_record, Inv_Log_Max_Len) == Inv_Log_Max_Len) {
        // 如果 since_timestamp 早于文件中最旧的记录，则所有记录都是新的
        if (since_timestamp < rtc_time_to_time_t(&temp_record.LogTime)) {
            ESP_LOGD(TAG, "since_timestamp is older than the oldest record, counting all records.");
            return IotSetData.dev_info_t.historyRecSaveCount;
        }
    } else {
        ESP_LOGE(TAG, "Failed to read oldest record at index %u for pre-check.", oldest_record_p_idx);
        return 0; // 关键记录读取失败，无法继续
    }

    // 在逻辑上，最新的记录是第 (current_records - 1) 条。
    uint16_t newest_record_l_idx = IotSetData.dev_info_t.historyRecSaveCount - 1;
    // 其物理索引 = (最旧记录的物理索引 + 最新记录的逻辑索引) % 数组总容量
    uint16_t newest_record_p_idx = (oldest_record_p_idx + newest_record_l_idx) % HISTORY_LOG_RECORD_MAX_NUM;
    
    record_offset = newest_record_p_idx * Inv_Log_Max_Len;
    if (fs_file_read_at(path, record_offset, (uint8_t*)&temp_record, Inv_Log_Max_Len) == Inv_Log_Max_Len) {
        // 如果 since_timestamp 晚于文件中最新的记录，则所有记录都是旧的
        if (since_timestamp >= rtc_time_to_time_t(&temp_record.LogTime)) {
            ESP_LOGD(TAG, "since_timestamp is newer than or equal to the newest record, no new records.");
            return 0;
        }
    } else {
        ESP_LOGE(TAG, "Failed to read newest record at index %u for pre-check.", newest_record_p_idx);
        return 0; // 关键记录读取失败，无法继续
    }

    // --- 4. 二分查找：定位第一个新记录 ---
    // 在逻辑索引 [0, current_records - 1] 上进行二分查找。
    int low = 0, high = IotSetData.dev_info_t.historyRecSaveCount - 1;
    int first_new_l_idx = -1; // 第一个新记录的逻辑索引

    while (low <= high) {
        int mid_l_idx = low + (high - low) / 2; // 中间点的逻辑索引

        // 将逻辑索引转换为物理文件索引
        // 当前物理索引 = (最旧记录的物理索引 + 当前逻辑索引) % 数组总容量
        uint16_t mid_p_idx = (oldest_record_p_idx + mid_l_idx) % HISTORY_LOG_RECORD_MAX_NUM;
        
        record_offset = mid_p_idx * Inv_Log_Max_Len;
        if (fs_file_read_at(path, record_offset, (uint8_t*)&temp_record, Inv_Log_Max_Len) != Inv_Log_Max_Len) {
            ESP_LOGE(TAG, "Binary search failed to read record at physical index %u", mid_p_idx);
            return 0; // 查找失败
        }

        if (rtc_time_to_time_t(&temp_record.LogTime) > since_timestamp) {
            // 这是一个新记录。
            // 找到了一个潜在的边界，记录它，并尝试在更早的部分（左半区）寻找更早的新记录。
            first_new_l_idx = mid_l_idx;
            high = mid_l_idx - 1;
        } else {
            // 这是一个旧记录。
            // 在更晚的部分（右半区）寻找新记录。
            low = mid_l_idx + 1;
        }
    }

    // 5. 计算并返回结果
    if (first_new_l_idx != -1) {
        // 新记录的数量 = 总记录数 - 第一个新记录的逻辑索引
        return IotSetData.dev_info_t.historyRecSaveCount - first_new_l_idx;
    }

    return 0; // 没有找到任何新记录
}

/**
 * @brief      读取增量日志数据到动态分配的缓冲区。
 * @details    此函数是实现日志增量同步的核心接口。它会：
 *             1. 调用辅助函数  计算出新记录的数量。
 *             2. 根据新记录数量计算总大小（包含文件头），并分配相应大小的内存。
 *             3. 从文件开头读取文件头，然后从文件中读取所有新记录，
 *                并将它们按时间顺序（旧->新）存入缓冲区。
 *             4. 通过输出参数返回缓冲区地址和总大小。
 *
 * @param[in]  since_timestamp  一个UNIX时间戳，用于界定“新”记录。
 * @param[out] out_buffer       一个指向 `uint8_t*` 的指针。如果函数成功，
 *                              它将被设置为新分配的缓冲区的地址。
 * @param[out] out_size         一个指向 `uint32_t` 的指针。如果函数成功，
 *                              它将被设置为缓冲区的总大小（字节）。
 *
 * @return     int
 *             - 0: 成功 (即使没有新记录也返回成功)。
 *             - -1: 失败（如内存分配失败、文件读取失败等）。
 *
 * @note       **重要**: 调用者在处理完 `out_buffer` 中的数据后，
 *             **必须**负责调用 `free(out_buffer)` 来释放内存。
 */
int Inv_Log_Read_Incremental(time_t since_timestamp, uint8_t **out_buffer, uint32_t *out_size)
{
    // 1. 参数校验
    if (out_buffer == NULL || out_size == NULL) {
        return -1;
    }
    
    // 获取文件操作互斥锁，保护整个读操作过程
    if (xInvLogFileProcessSemaphore && xSemaphoreTake(xInvLogFileProcessSemaphore, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "Failed to take file process semaphore for reading.");
        return -1;
    }

    int ret = 0;
    uint8_t *buffer = NULL;
    
    *out_buffer = NULL;
    *out_size = 0;

    // 2. 调用辅助函数，获取新记录的数量
    uint16_t new_records_count = Inv_Log_Count_New_Records(since_timestamp);

    // 如果没有新记录，则无需分配内存和读取，直接返回成功
    if (new_records_count == 0) {
        ESP_LOGW(TAG, "No new records found since timestamp %lld.", since_timestamp);
        goto exit_point; 
    }

    ESP_LOGI(TAG, "Found %u new records since timestamp %lld. Preparing to read.", new_records_count, since_timestamp);

    // 3. 计算总大小并分配内存
    size_t Inv_Log_Max_Len = sizeof(LOG_FAULT_STRUCT);
    uint32_t total_size = new_records_count * Inv_Log_Max_Len;
    buffer = (uint8_t *)iot_calloc(total_size);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %lu bytes for incremental log.", total_size);
        ret = -1;
        goto exit_point;
    }

    // 4. 读取所有新记录到缓冲区
    char path[30] = {0};
    sprintf(path, "%s/%s%s%d_%02d", FS_BASE_PATH, LOG_RECORD_FOLDER_LEVEL1, INV_RECORD_FILE_MARK, 0, 0);
    //    首先，找到文件中第一个新记录的物理位置
    uint16_t oldest_record_p_idx; // 最旧记录的物理索引
    if (IotSetData.dev_info_t.historyRecSaveCount < HISTORY_LOG_RECORD_MAX_NUM) {
        // 文件未满，最旧的记录在索引0
        oldest_record_p_idx = 0;
    } else {
        // 文件已满，最旧的记录在当前的写指针位置
        oldest_record_p_idx = IotSetData.dev_info_t.historyAddrIndex;
    }
    // 第一个新记录的逻辑索引 = 总记录数 - 新记录数
    uint16_t first_new_l_idx = IotSetData.dev_info_t.historyRecSaveCount - new_records_count;
    // 第一个新记录的物理索引 = (最旧记录的物理索引 + 当前逻辑索引) % 数组总容量
    uint16_t first_new_p_idx = (oldest_record_p_idx + first_new_l_idx) % HISTORY_LOG_RECORD_MAX_NUM;

    // 分一或两块，将所有新记录顺序读入缓冲区
    uint8_t *record_write_ptr = buffer;
    uint16_t space_to_end = HISTORY_LOG_RECORD_MAX_NUM - first_new_p_idx;

    if (new_records_count <= space_to_end) {
        // 情况A: 所有新记录都在一个连续的块中
        uint32_t read_offset = first_new_p_idx * Inv_Log_Max_Len;
        if (fs_file_read_at(path, read_offset, record_write_ptr, new_records_count * Inv_Log_Max_Len) != new_records_count * Inv_Log_Max_Len) {
            ESP_LOGE(TAG, "Failed to read continuous block of new records.");
            ret = -1;
            goto exit_point;
        }
    } else {
        // 情况B: 新记录跨越了物理文件的末尾，需要分两次读取
        uint16_t cnt1 = space_to_end;
        uint16_t cnt2 = new_records_count - cnt1;

        // 读取第一部分 (从 first_new_p_idx 到文件末尾)
        uint32_t read_offset1 = first_new_p_idx * Inv_Log_Max_Len;
        if (fs_file_read_at(path, read_offset1, record_write_ptr, cnt1 * Inv_Log_Max_Len) != cnt1 * Inv_Log_Max_Len) {
            ESP_LOGE(TAG, "Failed to read first part of wrapped records.");
            ret = -1;
            goto exit_point;
        }

        // 读取第二部分 (从文件开头到剩余记录结束)
        uint32_t read_offset2 = 0; // 从记录区开头读
        if (fs_file_read_at(path, read_offset2, record_write_ptr + cnt1 * Inv_Log_Max_Len, cnt2 * Inv_Log_Max_Len) != cnt2 * Inv_Log_Max_Len) {
            ESP_LOGE(TAG, "Failed to read second part of wrapped records.");
            ret = -1;
            goto exit_point;
        }
    }

    // 6. 成功，返回缓冲区地址和大小
    *out_buffer = buffer;
    *out_size = total_size;

exit_point:
    // 统一的出口：处理资源释放
    if (ret != 0 && buffer != NULL) {
        // 如果函数执行失败，且内存已分配，则释放内存
        free(buffer);
        *out_buffer = NULL; // 确保外部不会使用悬空指针
        *out_size = 0;
    }

    // 释放文件操作互斥锁
    xSemaphoreGive(xInvLogFileProcessSemaphore);
    
    return ret;
}

/*------------------------------------------------------------------------------
 Function: Is_Inv_Log_File
 -----------------------------------------------------------------------------*/
/**
  * @brief      检查是否为逆变告警文件
  * @param[in]  const char *fname  
  * @param[out] None
  * @return     int
  */
int Is_Inv_Log_File(const char *fname)
{
    // 1. 参数校验
    if (fname == NULL) {
        return -1;
    }

    // 2. 根据传入的参数构建请求的文件路径
    //    fname[0] 是目录, &fname[1] 是文件名
    char request_path[40] = {0};
    char filedir = fname[0];
    char* filename = &fname[1];

    if ( (LOG_RECORD_FOLDER_LEVEL1)[0] != filedir ) {
        // 目录不匹配，不是目标文件
        // ESP_LOGE(TAG, "dir is error ：%c.", filedir);
        return -1;
    }
    
    // 使用 snprintf 安全地构建路径，防止溢出
    snprintf(request_path, sizeof(request_path), "%s/%s%s", FS_BASE_PATH, LOG_RECORD_FOLDER_LEVEL1, filename);

    // 3. 使用与日志模块内部完全相同的宏来生成标准文件名
    char local_log_path[40] = {0};
    sprintf(local_log_path, "%s/%s%s%d_%02d", FS_BASE_PATH, LOG_RECORD_FOLDER_LEVEL1, INV_RECORD_FILE_MARK, 0, 0);

    // 4. 比较两个路径字符串
    if (strcmp(request_path, local_log_path) == 0) {
        // 两个字符串完全相同，确认是目标日志文件
        return 0;
    }

    // 字符串不匹配，不是目标文件
    // ESP_LOGE(TAG, "request_path is error ：%s.", request_path);
    return -1;
}

/**
 * @brief 生成当前日志文件对应的文件标识符。
 *
 * - out_buf[0] = 文件夹层级字符
 * - &out_buf[1] = 文件名字符串（不含路径分隔符）
 *
 * @param[out] out_buf  输出缓冲区，至少应能容纳 1 + 文件名长度
 * @param[in]  buf_len  out_buf 长度（字节）
 *
 * @return int
 *         - 0 : 成功（out_buf 被填充）
 *         - -1: 参数错误或缓冲区不足
 */
int Get_Inv_Log_File_Id(char *out_buf, size_t buf_len)
{
    if (out_buf == NULL || buf_len < 2) {
        return -1;
    }

    char local_log_path[30] = {0};
    sprintf(local_log_path, "%s/%s%s%d_%02d", FS_BASE_PATH, LOG_RECORD_FOLDER_LEVEL1, INV_RECORD_FILE_MARK, 0, 0);

    const char *folder_char = (LOG_RECORD_FOLDER_LEVEL1 && LOG_RECORD_FOLDER_LEVEL1[0]) ?
                                &LOG_RECORD_FOLDER_LEVEL1[0] : NULL;
    if (folder_char == NULL) {
        return -1;
    }

    // 提取文件名部分（最后一个 '/' 之后）
    const char *p = strrchr(local_log_path, '/');
    const char *filename = p ? p + 1 : local_log_path;
    size_t fnlen = strlen(filename);

    if (fnlen + 1 > buf_len) { // 1 字节 folder + fnlen
        return -1;
    }

    out_buf[0] = folder_char[0];
    memcpy(out_buf + 1, filename, fnlen); // 不包含终止符

    return 0;
}

/**
 * @brief 统计晚于指定时间戳的日志新记录数（含信号量保护）。
 *
 * @param[in] since_timestamp  自 Unix 纪元起的时间戳；传 0 表示统计全部记录。
 *
 * @return int
 *         - >=0 : 新记录数量
 *         - -1  : 参数错误或其它不可恢复错误
 *         - -2  : 获取文件处理信号量失败（记录将被丢弃）
 */
int Is_Inv_Log_Count_New(time_t since_timestamp)
{
    uint16_t new_count = 0;

    /* 获取信号量保护 */
    if (xInvLogFileProcessSemaphore && xSemaphoreTake(xInvLogFileProcessSemaphore, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "failed to take file process semaphore. Records will be discarded.");
        return -2;
    }

    /* 统计晚于 since_timestamp 的新记录数量（内部处理文件读错时返回0） */
    new_count = Inv_Log_Count_New_Records(since_timestamp);

    /* 释放信号量 */
    if (xInvLogFileProcessSemaphore) {
        xSemaphoreGive(xInvLogFileProcessSemaphore);
    }

    return (int)new_count;
}


/*故障位在历史记录中存储序号*/
#define HISTORY_INV_FAULT_START_INDEX       1   //1~6
#define HISTORY_INV_WARN_START_INDEX        7   //7~10
#define HISTORY_PACK_PROTECT_START_INDEX    50  //50~51
#define HISTORY_PACK_FAULT_START_INDEX      52  //52~55
//#define HISTORY_IOT_FAULT_START_INDEX

/*------------------------------------------------------------------------------
 Function: inv_fault_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      处理INV故障信息
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
static void inv_fault_check(uint8_t index)
{       
    uint8_t i = 0;
	uint8_t FaultNumber = 0;
	uint16_t dLastErrorCodeTemp    = 0;
	uint16_t dCurrentErrorCodeTemp = 0;
	static USE_EXT_RAM_BSS uint16_t PCSFault[(DEV_MAIN_NODE_MAX*INV_MAX_NUM)+1][6] = {0};//逆变器历史记录

    for(FaultNumber = 0; FaultNumber < 6; FaultNumber++)
    {
        /*判断是否有故障状态变化，一次只处理一个故障寄存器*/
        if (PCSFault[index][FaultNumber] != top_modbus_rd.Inv[index].mod_reg00100_AppPage1.fault[FaultNumber])
        {
        	for(i = 0; i < 16; i++)
			{  
				dLastErrorCodeTemp = PCSFault[index][FaultNumber] & (0x01 << i);
				dCurrentErrorCodeTemp = top_modbus_rd.Inv[index].mod_reg00100_AppPage1.fault[FaultNumber] & (0x01 << i);

				if ((dLastErrorCodeTemp == 0) && dCurrentErrorCodeTemp) 
				{
					inv_SaveOneErrorcode(1, FaultNumber + HISTORY_INV_FAULT_START_INDEX, dCurrentErrorCodeTemp, index); // 新产生故障
#ifdef INV_LOG_DETAILED_INFO_RECORD
                    SaveErrorDetailInfo(1, (FaultNumber * 16 + INV_FAULTCODE_BASE), dCurrentErrorCodeTemp, index);
#endif
				}
				else if (dLastErrorCodeTemp && (dCurrentErrorCodeTemp == 0)) 
				{
					inv_SaveOneErrorcode(0, FaultNumber + HISTORY_INV_FAULT_START_INDEX, dLastErrorCodeTemp, index); // 消除的故障
#ifdef INV_LOG_DETAILED_INFO_RECORD
                    SaveErrorDetailInfo(0, (FaultNumber * 16 + INV_FAULTCODE_BASE), dLastErrorCodeTemp, index);
#endif
				}
			}
            
			PCSFault[index][FaultNumber] = top_modbus_rd.Inv[index].mod_reg00100_AppPage1.fault[FaultNumber];
        }
    }    
}

/*------------------------------------------------------------------------------
 Function: inv_alarm_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      处理INV告警信息
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
static void inv_alarm_check(uint8_t index)
{   
    uint8_t i = 0;
	uint8_t AlarmNumber = 0;
	uint16_t dLastErrorCodeTemp    = 0;
	uint16_t dCurrentErrorCodeTemp = 0;
	static USE_EXT_RAM_BSS uint16_t PCSAlarm[(DEV_MAIN_NODE_MAX*INV_MAX_NUM)+1][4] = {0};//逆变器历史记录

    for(AlarmNumber = 0; AlarmNumber < 4; AlarmNumber++)
    {
        /*判断是否有告警状态变化，一次只处理一个告警寄存器*/
        if (PCSAlarm[index][AlarmNumber] != top_modbus_rd.Inv[index].mod_reg00100_AppPage1.alarm[AlarmNumber])
        {
        	for(i = 0; i < 16; i++)
			{  
				dLastErrorCodeTemp = PCSAlarm[index][AlarmNumber] & (0x01 << i);
				dCurrentErrorCodeTemp = top_modbus_rd.Inv[index].mod_reg00100_AppPage1.alarm[AlarmNumber] & (0x01 << i);

				if ((dLastErrorCodeTemp == 0) && dCurrentErrorCodeTemp) 
				{
					inv_SaveOneErrorcode(1, AlarmNumber + HISTORY_INV_WARN_START_INDEX, dCurrentErrorCodeTemp, index); // 新产生告警
#ifdef INV_LOG_DETAILED_INFO_RECORD
                    SaveErrorDetailInfo(1, (AlarmNumber * 16 + INV_ALARMCODE_BASE), dCurrentErrorCodeTemp, index);
#endif
				}
				else if (dLastErrorCodeTemp && (dCurrentErrorCodeTemp == 0)) 
				{
					inv_SaveOneErrorcode(0, AlarmNumber + HISTORY_INV_WARN_START_INDEX, dLastErrorCodeTemp, index); //消除的告警
#ifdef INV_LOG_DETAILED_INFO_RECORD
                    SaveErrorDetailInfo(0, (AlarmNumber * 16 + INV_ALARMCODE_BASE), dLastErrorCodeTemp, index);
#endif					
				}
			}
            
			PCSAlarm[index][AlarmNumber] = top_modbus_rd.Inv[index].mod_reg00100_AppPage1.alarm[AlarmNumber];
        }
    }
}

/*------------------------------------------------------------------------------
 Function: pack_protect_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      处理PACK保护信息
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
static void pack_protect_check(void)
{   
    uint8_t i = 0;
	uint8_t ProtectNumber = 0;
	uint16_t dLastErrorCodeTemp    = 0;
	uint16_t dCurrentErrorCodeTemp = 0;
	static USE_EXT_RAM_BSS uint16_t PCSProtect[2] = {0};//PACK历史记录

    for(ProtectNumber = 0; ProtectNumber < 2; ProtectNumber++)
    {
        /*判断是否有告警状态变化，一次只处理一个告警寄存器*/
        uint16_t NowProtectCode = 0;
        switch ( ProtectNumber )
        {
            case 0 :
                NowProtectCode = top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg06000_Pack_sum.all_pack_alarm1;
                break;
            case 1 :
                NowProtectCode = top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg06000_Pack_sum.all_pack_alarm2;
                break;
            default:
                return;
        }
        
        if (PCSProtect[ProtectNumber] != NowProtectCode)
        {
        	for(i = 0; i < 16; i++)
			{  
				dLastErrorCodeTemp = PCSProtect[ProtectNumber] & (0x01 << i);
				dCurrentErrorCodeTemp = NowProtectCode & (0x01 << i);

                /*不单独建立文件，共用逆变故障文件*/
				if ((dLastErrorCodeTemp == 0) && dCurrentErrorCodeTemp) 
				{
					inv_SaveOneErrorcode(1, ProtectNumber + HISTORY_PACK_PROTECT_START_INDEX, dCurrentErrorCodeTemp, DEV_MAIN_NODE_MAX*INV_MAX_NUM); // 新产生告警
				}
				else if (dLastErrorCodeTemp && (dCurrentErrorCodeTemp == 0)) 
				{
					inv_SaveOneErrorcode(0, ProtectNumber + HISTORY_PACK_PROTECT_START_INDEX, dLastErrorCodeTemp, DEV_MAIN_NODE_MAX*INV_MAX_NUM); //消除的告警
				}
			}
            
			PCSProtect[ProtectNumber] = NowProtectCode;
        }
    }
}


/*------------------------------------------------------------------------------
 Function: pack_single_fault_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      处理PACK单包信息
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void pack_single_fault_check(uint8_t idx)
{   
#ifdef HISTORY_PACK_FAULT_START_INDEX

    uint8_t i = 0;
	uint8_t index = 0;
	uint16_t dLastErrorCodeTemp    = 0;
	uint16_t dCurrentErrorCodeTemp = 0;
	static USE_EXT_RAM_BSS uint16_t PackStatus[PACK_MAX_NUM][4] = {0};//PACK历史记录

    for(index = 0; index < 4; index++)
    {
        /*判断是否有告警状态变化，一次只处理一个告警寄存器*/
        uint16_t NowStatus = 0;

        /*52~55：电池故障（P系列告警码，6146~6149）*/
        switch ( index )
        {
            case 0 :
                NowStatus = top_modbus_rd.Pack[idx].mod_reg06100_Pack_each.SysErr[0];
                break;
            case 1 :
                NowStatus = top_modbus_rd.Pack[idx].mod_reg06100_Pack_each.SysErr[1];
                break;
            case 2 :
                NowStatus = top_modbus_rd.Pack[idx].mod_reg06100_Pack_each.SysErr[2];
                break;
            case 3 :
                NowStatus = top_modbus_rd.Pack[idx].mod_reg06100_Pack_each.alarm1;
                break;
            default:
                return;
        }
        
        if (PackStatus[idx][index] != NowStatus)
        {
        	for(i = 0; i < 16; i++)
			{  
				dLastErrorCodeTemp = PackStatus[idx][index] & (0x01 << i);
				dCurrentErrorCodeTemp = NowStatus & (0x01 << i);

                /*不单独建立文件，共用逆变故障文件*/
				if ((dLastErrorCodeTemp == 0) && dCurrentErrorCodeTemp) 
				{
					inv_SaveOneErrorcode(1, index + HISTORY_PACK_FAULT_START_INDEX, dCurrentErrorCodeTemp, DEV_MAIN_NODE_MAX*INV_MAX_NUM); // 新产生告警
				}
				else if (dLastErrorCodeTemp && (dCurrentErrorCodeTemp == 0)) 
				{
					inv_SaveOneErrorcode(0, index + HISTORY_PACK_FAULT_START_INDEX, dLastErrorCodeTemp, DEV_MAIN_NODE_MAX*INV_MAX_NUM); //消除的告警
				}
			}
            
			PackStatus[idx][index] = NowStatus;
        }
    }
    
#endif    
}

/*------------------------------------------------------------------------------
 Function: dev_log_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      inv历史记录检查
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void dev_log_check(void)
{
    uint16_t addr = 0;

    /*逆变故障检查*/
    inv_fault_check(DEV_MAIN_NODE_MAX*INV_MAX_NUM);
    
    /*逆变告警检查*/
    inv_alarm_check(DEV_MAIN_NODE_MAX*INV_MAX_NUM);
    
    /*多逆变并机时单机分别检查*/
    if (1 < (DEV_MAIN_NODE_MAX*INV_MAX_NUM))
    {   
        /*单机检查*/
        for (addr = 0; addr < (DEV_MAIN_NODE_MAX*INV_MAX_NUM); addr++)
        {   
            /*逆变故障检查*/
            inv_fault_check(addr);
            
            /*逆变告警检查*/
            inv_alarm_check(addr);
        }
    }

    /*PACK保护检查*/
    pack_protect_check();

#if 0
    /*内置电池包告警检查*/
    pack_single_fault_check(0);
#endif

    /*从已保存到缓存的历史记录队列写入到flash*/
    Logger_Pop();
}

