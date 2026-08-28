#include "dev_data_record.h"
//#include "historic_data_record.h"
//#include "drv_gd25q128.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include "iot_period_task.h"
#include "uart_device_process.h"
#include "can_protocol.h"
#include "filesystem.h"
#include "server2internet.h"
#include "iot_box_task.h"
#include "esp_wifi_types.h"
#include "tou_relay_ctrl.h"
#include "utils.h"

static const char *TAG = "[DEV_DATA_RECORD]";

// 文件目录链表中文件名称的指针;从小到大;[x]-链表元素的顺序
EXT_RAM_BSS_ATTR name_list_t2 *fsys_file_list;


//modbus 3000历史记录
QueueHandle_t xQueue_Log_record = NULL;//CAN底层 Rx队列
#define HISTORY_MAX_COUNT 		 (100)	//最大历史记录数
#define HISTORY_ONE_BYTE_COUNT 	 (sizeof(reals.log_fault_info)) //一条历史记录的存储字节长度，10B


//windy add 
QueueHandle_t xQueue_iot_historydata_record = NULL;//IOT event本地记录
#define HISTORY_MAX_COUNT_2 		 (100)	//最大历史记录数
#define HISTORY_ONE_BYTE_COUNT_2 	 (sizeof(reals.HistoryData_event)) //一条历史记录的存储字节长度，10B


#define MAX_PATH_LEN     256   // 最大路径长度限制
#define MAX_FILENAME_LEN 255   // 文件系统名最大长度 

/*
20240510
暂定简化文件目录树
只定义一个子目录：A-表示X
A目录下基于年月日作为文件名记录内容：YY-MM-DD,共计占用6个字节作为文件名，空低3字节


关于文件名的获取，上级仅发一次路径请求命令，下级一次把设备端的多个目录和每个目录中的多个文件通过 10字节（目录+文件名=1+9）的方法顺序填充，全部上传
下次，上级只需要查询指定的10字节文件名即可


*/

/* 用于保存查询文件或目录时的数据 */
//static historic_file_query_t file_query;//链表，开头是内存起始地址，结束是NULL指针

static void dump_buf(char *info, uint8_t *buf, uint32_t len)
{
    printf("%s", info);
    for (int i = 0; i < len; i++) {
        printf("%s%02X%s", i % 16 == 0 ? "\n     ":" ", 
                        buf[i], i == len - 1 ? "\n":"");
    }
}


/*
基于指定的文件名目录、内容偏移地址、返回数据内容和剩余长度
input:fname
input:offset

output:data
output:size

*/
uint8_t* debug_print_data_from_storage( uint32_t offset)
//uint8_t* debug_print_data_from_storage(char *fname, uint32_t offset)
{
	uint8_t *data;
	uint32_t size;
//	char *fname;
	char fname[30] = {0};
	INV_FILE_PATH(fname,1,1);

	// 从存储器中获取数据

	/* 获取从指文件定位置到文件结束的数据大小 */
	if(historic_data_get_size(fname, offset, &size) < 0)  
	{
		return NULL;
	}
    
	data = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
	if(data == NULL) 
	{
		return NULL;
	}

	/* 读取文件数据指定长度size到data */
	if(historic_data_read(fname, data, &size, offset) < 0) 
	{
	    free(data);
		data=NULL;
		return NULL;
	}
	else
	{

	
		dump_buf("print_data_from_storage", (uint8_t *)data, size);
	    free(data);
		data=NULL;

	}

    return data;
}

#if 0

/**
 * @brief 解析历史记录指令，RX解析
 *
 * @param cmd_buf 历史记录指令缓存指针
 * @param len 数据缓存长度
 * @param record_cmd 用于保存解析后的数据
 *
 * @return 成功返回0,否则返回-1
 */
int historic_record_parse_cmd(uint8_t *cmd_buf, uint8_t len, historic_record_cmd_t *record_cmd)
{
	if(cmd_buf == NULL || len < RECORD_QUERY_CMD_LEN) return -1;

	uint8_t query_cmd[FILE_DIR_LEN-1] = {0};
	memset(record_cmd->file_name, 0x00, sizeof(record_cmd->file_name));
	record_cmd->op_mode = RECORD_OP_ERROR;

	/* 根目录操作 */
	if(cmd_buf[0] == ROOT_DIR_FLAG)//根目录空
	{
		/* 查询根目录下的历史数据文件及文件夹 */
		if(memcmp(&cmd_buf[1], query_cmd, (FILE_DIR_LEN-1)) == 0) //文件名空
		{
			record_cmd->op_mode = RECORD_OP_QUERY_ROOT_DIR;
		}
		/* 获取根目录下的历史数据文件 */
		else  //文件名非空
		{
			record_cmd->op_mode = RECORD_OP_GET_ROOT_DATA;
		}
		record_cmd->dir_name = cmd_buf[0];
		memcpy(record_cmd->file_name, &cmd_buf[1], (FILE_DIR_LEN-1));
	}
	/* 子目录操作 */
	else if(IS_RECORD_CMD_SUB_DIR(cmd_buf[0]))//根目录非空
	{
		/* 查询子目录下的历史数据 */
		if(memcmp(&cmd_buf[1], query_cmd, (FILE_DIR_LEN-1)) == 0) //文件名空
		{
			record_cmd->op_mode = RECORD_OP_QUERY_SUB_DIR;
		}
		/* 获取子目录下的历史数据文件 */
		else  //文件名非空
		{
			record_cmd->op_mode = RECORD_OP_GET_SUB_DATA;
		}
		memcpy(record_cmd->file_name, &cmd_buf[1], (FILE_DIR_LEN-1));
	}
	else 
	{
		return -1;
	}

	/* 如果是获取数据文件,设置数据在文件中的偏移 */
	record_cmd->offset = *(int*)&cmd_buf[FILE_DIR_LEN];

	/* 设置是否支持连续操作,即设备收到查询指令后自动将文件剩余数据全部发送到上层服务
	 * 非连续操作模式下,设备每接收到一次查询指令响应本次指令获取的数据大小而非全部文件剩余数据 */
	record_cmd->continuous = cmd_buf[FILE_DIR_LEN+sizeof(record_cmd->offset)];
	return 0;
}

/**
 * @brief 序列化历史数据传输
 *
 * 该函数将历史数据序列化为协议中指定的数据格式
 *
 * @param record_data 待序列化的历史数据结构指针
 * @param buf 用于保存序列化后的数据缓存
 * @param len 数据缓存长度
 *
 * @return 成功返回序列化后的数据长度,否则返回-1
 */
int historic_record_serialize_transmission(historic_record_data_t *record_data, uint8_t *buf, uint32_t len)
{
	if(buf == NULL || len == 0) return -1;
	if(record_data->total == 0 || record_data->len < record_data->total || record_data->data == NULL) return -1;
	if(record_data->len > SINGLE_TRANSMISSION_MAX_LEN) return -1;

	uint8_t *p_buf = buf;
	int idx = 0;

	*(uint16_t*)(p_buf+idx) = record_data->class;				// 数据分类
	idx += sizeof(record_data->class);

	*(uint32_t*)(p_buf+idx) = record_data->total;				// 数据总数
	idx += sizeof(record_data->total);

	*(uint16_t*)(p_buf+idx) = record_data->len;					// 当前数据长度
	idx += sizeof(record_data->len);

	*(uint32_t*)(p_buf+idx) = record_data->offset;				// 当前数据偏移
	idx += sizeof(record_data->offset);

	memcpy((p_buf+idx), record_data->data, record_data->len);	// 当前数据
	idx += record_data->len;
	return idx;
}


#endif

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
	    if (idx != 0)
	    {
	        buf[idx] = 0x2C;    //逗号分隔
	        idx++;
	    }
        
		memcpy(&buf[idx], fsys_file_list[i].name, FILE_DIR_LEN);

		idx += FILE_DIR_LEN;
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
 * @brief setdata数据写入到file
 *
 *
 * @param fname 待写入的文件名,名称中包含除根目录的文件路劲
 * @param record 历史数据指针
	uint32_t file_bias:指定操作的偏移条数：0-从头开始；其他变量-从偏移位置开始覆盖写操作 
 * @param len :待写入数据长度
 *
 * @return 成功返回0,否则返回-1

两种写入方法
 尾部追加写入；
 或指定中间偏移写入
 */
int SetData_file_data_write(char *fname, void *record, uint32_t file_bias, uint32_t len)
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
        uint32_t file_pos = file_bias * 1;
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

    if(fname[0] == 0) //查询根目录
	{	
		sprintf(path, "%s/%s", RECORD_ROOT_PATH, &fname[1]);
	}
	else //查询子目录
	{				
		sprintf(path, "%s/%c/%s", RECORD_ROOT_PATH, fname[0], &fname[1]);
	}

    ESP_LOGW(TAG, "[historic_data_get_size] read path : %s", path);

	/* 只读格式打开文件 */
	int fd = open(path, O_RDONLY);
	if(fd < 0)
    {
        ESP_LOGE(TAG, "[historic_data_get_size] open error(%s)",path);
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
    
    if(fname[0] == 0) //查询根目录
	{	
		sprintf(path, "%s/%s", RECORD_ROOT_PATH, &fname[1]);
	}
	else //查询子目录
	{				
		sprintf(path, "%s/%c/%s", RECORD_ROOT_PATH, fname[0], &fname[1]);
	}


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
int historic_data_bias_read(char *fname, void *record, uint32_t *len, uint32_t file_pos)
{
	if(fname == NULL || record == NULL || *len == 0) 
    {
        ESP_LOGE(TAG, "[historic_data_bias_read] error");
        return -1;
    }

	char path[30] = {0};
    uint16_t file_bias = 0;
    uint32_t log_size = 0;
    uint32_t max_size = 0;
    uint32_t read_size = 0;//读取长度

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
    if (strncmp(&fname[1], PARAMETER_FILE_MARK_IOT, 2) == 0)//事件
    {
        file_bias = SetData.dev_info_t.Event_AddrIndex;
        max_size = HISTORY_MAX_COUNT * sizeof(EventHistoryData_Struct);
        log_size = sizeof(EventHistoryData_Struct);
        ESP_LOGW(TAG, "[iot historic_data_bias_read] %s, bia = %d", fname, file_bias);
    }
    else if (strncmp(&fname[1], PARAMETER_FILE_MARK_INV, 2) == 0)//INV历史记录事件：L/inv1_1
    {
        file_bias = SetData.dev_info_t.historyAddrIndex;
        max_size = HISTORY_MAX_COUNT * sizeof(reals.log_fault_info);
        log_size = sizeof(reals.log_fault_info);
        ESP_LOGW(TAG, "[inv historic_data_bias_read] %s, bia = %d", fname, file_bias);
    }
	#ifdef INV_LOG_DETAILED_INFO_RECORD
	else if(strncmp(&fname[1], INV_BOX_FILE_NAME, 2) == 0)
	{
		ESP_LOGI(TAG,"fname:%s,fname:%d,%d",fname,fname[4],fname[5]);

		uint8_t index=0;
		if((fname[5] >= 0x30)&&(fname[5] <= 0x39))//0x30="0""0x39"=9
		{
			index=fname[5]-0x30;//,ASCII转换为数字
		}
		else
		{
			index=0;//default 0,
		}	

		//Inv[reals.Addr_can_self].mod_reg12000_IOT_set.history_index =SetData.dev_info_t.invDetailedInfo_SaveCount[reals.Addr_can_self+1];
		//Inv[reals.Addr_can_self].mod_reg12000_IOT_set.history_num =SetData.dev_info_t.invDetailedInfo_AddrIndex[reals.Addr_can_self+1];
			
		
		file_bias = SetData.dev_info_t.invDetailedInfo_AddrIndex[index];
        max_size = INV_DETAILED_INFO_MAX_COUNT * sizeof(Inv_Detailed_Info_Datas);
        log_size = sizeof(Inv_Detailed_Info_Datas);
        ESP_LOGW(TAG, "[inv detailed_info_data_bias_read] %s, bia = %u", fname, file_bias);
	}
	#endif
//    else if ( strncmp(&fname[1], IOT_FILE_TYPE_LOG, 2) == 0 )//IOT告警
//    {
//        file_bias = IotSetData.iot_dev_info_t.iot_log_info.log_nums_bia;
//        max_size = HISTORY_LOG_RECORD_MAX_NUM * sizeof(reals.log_fault_info);
//        log_size = sizeof(reals.log_fault_info);
//        ESP_LOGW(TAG, "[historic_data_bias_read] %s, bia = %d", fname, file_bias);
//    }
//    else if ( strncmp(&fname[1], INV_FILE_TYPE_LOG, 2) == 0 )//逆变告警
//    {
//        size_t fnlen = strlen(fname);
//        int group_index = 0;
//        int ingroup_index = 0;
//        
//        // 检查 fname 的长度是否至少为 4
//        if (fnlen >= 4) 
//        {
//            extract_numbers(&fname[fnlen - 4], group_index, ingroup_index);
//            if((group_index < 0) || (ingroup_index < 0))
//            {
//                ESP_LOGE(TAG, "[historic_data_bias_read] file name error (%s)", fname);
//                return -1;
//            }
//            else
//            {
//                file_bias = InvSetData.inv_dev_info_t.inv_log_info[group_index * BIND_POINT_IN1ARRAY_MAX + ingroup_index].log_nums_bia;
//                max_size = HISTORY_LOG_RECORD_MAX_NUM * sizeof(reals.log_fault_info);
//                log_size = sizeof(reals.log_fault_info);
//                ESP_LOGW(TAG, "[historic_data_bias_read] %s, bia = %d (%d,%d)", fname, file_bias, group_index, ingroup_index);
//            }
//        }
//        else
//        {
//            ESP_LOGE(TAG, "[historic_data_bias_read] file name len error (%s)", fname);
//            return -1;
//        }
//    }
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
            if(read(fd, (uint8_t *)record + abs(real_pos), read_size) != read_size)
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

#if 0
/**
 * @brief 释放文件或目录查询过程中占用的动态内存空间
 *
 * 文件与目录分别使用链表数据结构保存,数据使用完成后需要释放内存空间
 *
 * @return none
 */
static void historic_data_buf_clr(void)
{
//	name_list_t *list_tmp;
//	name_list_t *list_head;

	/* 释放目录列表中占用的动态内存 */
//	list_head = file_query.dir_list;
//	while(list_head)
//	{
//		list_tmp = list_head;
//		list_head = list_head->list;
//
//		free(list_tmp->name);
//		free(list_tmp);
//	}

	/* 释放文件列表中占用的动态内存 */
//	list_head = file_query.file_list;
//	while(list_head)
//	{
//		list_tmp = list_head;
//		list_head = list_head->list;
//
//		free(list_tmp->name);
//		free(list_tmp);
//	}
//
//	/* 清零文件查询变量 */
//	memset(&file_query, 0x00, sizeof(file_query));

if(NULL != fsys_file_list)
{
	free(fsys_file_list);
}
	
}

/**
 * @brief 查询历史数据文件与文件夹
 *
 * 该函数将会查询目标目录下的文件与文件夹,文件与目录分别使用链表数据结构保存,
 * 第一次调用时会分配内存空间以保存文件与目录数据,后续调用会释放上一次的内存空间后,重新分配新的空间来保存文件与目录数据
 *
 * @param fname 待查询的目录或文件名,名称中包含除根目录的目录路劲
 * @param query 用于指向查询到的数据结构
 *
 * @return 成功返回0,否则返回-1
 */
//int historic_data_file_get(char *fname, historic_file_query_t **query)
//{
//	if(fname == NULL) return -1;
//
//	char path[30] = {0};
//	if(fname[0] == 0) {	//查询根目录
//		sprintf(path, "%s", RECORD_ROOT_PATH);
//	}
//	else {				//查询子目录
//		sprintf(path, "%s/%s", RECORD_ROOT_PATH, fname);
//	}
//
//	DIR *dir = opendir(path);
//	if(dir == NULL) return -1;
//
//	historic_data_buf_clr();		// 释放目录与文件占用的动态内存
//
//	/* 定义目录与文件链表指针 */
//	name_list_t *dir_list = 0;
//	name_list_t *file_list = 0;
//
//	struct dirent *p_dir;
//	while((p_dir = readdir(dir)) != NULL)
//    {
//        if(strcmp(p_dir->d_name,".")==0 || strcmp(p_dir->d_name,"..")==0) { // 是当前目录或父目录则continue
//			continue;
//		}
//
//		int name_len = strlen(p_dir->d_name);
//		name_list_t *list = malloc(sizeof(name_list_t));
//		if(list == NULL) goto __error;
//		char *_name = malloc(name_len+1);
//		if(_name == NULL)
//		{
//			free(list);
//			goto __error;
//		}
//
//		strcpy(_name, p_dir->d_name);
//		list->name = _name;
//		sscanf(list->name,"%s*[^/]%hd",list->filename_value);
//		
//		list->list = NULL;
//
//		if(p_dir->d_type == DT_DIR)//目录名
//		{
//			file_query.dir_nums++;
//			if(dir_list == NULL)
//			{
//				file_query.dir_list = list;		// 初始化目录指针链表头
//				dir_list = list;				// 目录链表指针指向初始值
//			}
//			else
//			{
//				dir_list->list = list;			// 串联上个链表与当前链表
//				dir_list = list;				// 目录链表指针指向当前链表
//			}
//		}
//		else//文件名
//		{
//			file_query.file_nums++;
//			if(file_list == NULL)//首次初始化，执行链表内存区开始地址
//			{
//				file_query.file_list = list;	// 初始化文件指针链表头
//				file_list = list;				// 文件链表指针指向初始值
//			}
//			else
//			{
//				file_list->list = list;			// 串联上个链表与当前链表
//				file_list = list;				// 文件链表指针指向当前链表
//			}
//		}
//    }
//
//	*query = &file_query;
//	closedir(dir);
//	return 0;
//
//__error:
//	closedir(dir);
//	historic_data_buf_clr();
//	return -1;
//}

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
    
	if(fname[0] == 0) //查询根目录
	{	
		sprintf(path, "%s", RECORD_ROOT_PATH);
	}
	else //查询子目录
	{				
		sprintf(path, "%s/%s", RECORD_ROOT_PATH, fname);
	}

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
    
//	historic_data_buf_clr();		// 释放目录与文件占用的动态内存
	if(NULL != fsys_file_list)
	{
		free(fsys_file_list);
	}	
    
	fsys_file_list = heap_caps_malloc(reals.file_nums*sizeof(name_list_t2) , MALLOC_CAP_SPIRAM);//FILE_DIR_LEN

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
            //ESP_LOGW(TAG, "[file_list_clear] path2 : %s", path2);
            dir2 = opendir(path2);
        	if(dir2 == NULL) 
            {
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
                     
                        }
                    }
                    else
                    {
                        remove(path3);
            
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
    
                }
                
                #else
                
                if (strncmp(&fname[1], target_file_type, 2) == 0)
                {
                    remove(path3);
                    //ESP_LOGW(TAG, "[file_list_clear] delete : %s", path3);
                }

                #endif
            }
            else
            {
                remove(path3);
            }
		}
    }

	closedir(dir);
    
	return 0;
}
//
///*------------------------------------------------------------------------------
// Function: dev_file_delete
// -----------------------------------------------------------------------------*/
///**
//  * @brief      初始化，删除spi flash文件系统中指定
//设备文件（包括一级子目录内文件）
//  * @param[in]  target_file_type (null表示全部删除)  
//  * @param[out] None
//  * @return     int
//  */
//int dev_file_delete(uint8_t group_index, uint8_t ingroup_index)
//{
//	char path[30] = {0};
//    char path2[30] = {0};
//    char path3[30] = {0};
//    char folder[10] = {0};
//    char fname[10] = {0};
//
//    char target_dev_index[8] = {0};
//    
//    DIR *dir;
//    DIR *dir2;
//    struct dirent *p_dir;
//    struct dirent *p_dir2;
//
//    uint16_t indev = group_index * BIND_POINT_IN1ARRAY_MAX + ingroup_index;
//    // memset(&reals.inv_pv_energy[indev], 0, sizeof(reals.inv_pv_energy[indev]));
//    // memset(&reals.inv_hour_energy[indev], 0, sizeof(reals.inv_hour_energy[indev]));
//    // memset(&reals.inv_hour_energy_time[indev], 0, sizeof(reals.inv_hour_energy_time[indev]));
//    memset(&reals.inv_log_fault_info[indev], 0, sizeof(reals.inv_log_fault_info[indev]));
//    // memset(&InvSetData.inv_dev_info_t.inv_kwh_info[indev], 0, sizeof(InvSetData.inv_dev_info_t.inv_kwh_info[indev]));
//    memset(&InvSetData.inv_dev_info_t.inv_log_info[indev], 0, sizeof(InvSetData.inv_dev_info_t.inv_log_info[indev]));
//
//    reals.EpromFlag.sBit.InvKwhUpdate = 1;
//    reals.EpromFlag.sBit.InvLogUpdate = 1;
//    
//	sprintf(path, "%s", RECORD_ROOT_PATH);
//    snprintf(target_dev_index, sizeof(target_dev_index), "%d_%02d", group_index, (ingroup_index + 1));
//
//	dir = opendir(path);
//	if(dir == NULL) 
//    {
//        ESP_LOGE(TAG, "[dev_file_delete] opendir error");
//        return -1;
//    }
//
//	while((p_dir = readdir(dir)) != NULL)//遍历指定目录路径下的所有文件，包含子目录的子文件
//    {
//        if(strcmp(p_dir->d_name,".")==0 || strcmp(p_dir->d_name,"..")==0) 
//		{ // 是当前目录或父目录则continue
//			continue;
//		}
//
//		if(p_dir->d_type == DT_DIR)//目录名
//		{
//		    memcpy(folder, p_dir->d_name, sizeof(folder));
//            sprintf(path2, "%s/%s", RECORD_ROOT_PATH, folder);
//            dir2 = opendir(path2);
//        	if(dir2 == NULL) 
//            {
//                continue;
//            }
//        	while((p_dir2 = readdir(dir2)) != NULL)//遍历指定目录路径下的所有文件，不包含子目录的子文件
//            {                
//                if(strcmp(p_dir2->d_name,".")==0 || strcmp(p_dir2->d_name,"..")==0) 
//        		{ // 是当前目录或父目录则continue
//        			continue;
//        		}
//
//        		if(p_dir2->d_type == DT_DIR)//目录名
//        		{
//                    //不考虑子目录下让包含目录的情况
//        		}
//        		else//文件名
//        		{
//        		    memcpy(fname, p_dir2->d_name, sizeof(fname));
//                    sprintf(path3, "%s/%s/%s", RECORD_ROOT_PATH, folder, fname);
//
//                    size_t len = strlen(fname);
//                    
//                    // 检查 fname 的长度是否至少为 4
//                    if (len >= 4) 
//                    {
//                        if (strncmp(&fname[len - 4], target_dev_index, 4) == 0) 
//                        {
//                            remove(path3);
//                        }
//                    }
//        		}
//            }
//            closedir(dir2);
//		}
//		else//文件名
//		{
//            memcpy(fname, p_dir->d_name, sizeof(fname));
//            sprintf(path3, "%s/%s", RECORD_ROOT_PATH, fname);
//            
//            size_t len = strlen(fname);
//            
//            // 检查 fname 的长度是否至少为 4
//            if (len >= 4) 
//            {
//                if (strncmp(&fname[len - 4], target_dev_index, 4) == 0) 
//                {
//                    remove(path3);
//                    //ESP_LOGW(TAG, "[dev_file_delete] delete : %s", path3);
//                }
//            }
//		}
//    }
//
//	closedir(dir);
//    
//	return 0;
//}


/*------------------------------------------------------------------------------
 Function: file_list_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      基于文件目录使能的格式检查和清除
  * @param[in]  void  
  * @param[out] None
  * @return     int
  */
int file_list_check(void)
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
        ESP_LOGE(TAG, "[file_list_check] opendir error");
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
		    #ifndef FILE_SYSTEM_DIRECTORY_ENABLE
            
		    memcpy(folder, p_dir->d_name, sizeof(folder));
            sprintf(path2, "%s/%s", RECORD_ROOT_PATH, folder);
            dir2 = opendir(path2);
        	if(dir2 == NULL) 
            {
                ESP_LOGE(TAG, "[file_list_check] opendir2 error");
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
                    remove(path3);
                    ESP_LOGW(TAG, "[file_list_check] delete : %s", path3);
        		}
            }
            closedir(dir2);
            // 删除空目录
            if (rmdir(path2) == 0) {
                //ESP_LOGW(TAG, "[file_list_check] Deleted directory: %s", path2);
            } else {
                //ESP_LOGE(TAG, "[file_list_check] Failed to delete directory: %s", path2);
            }
            
            #endif
		}
		else//文件名
		{
		    #ifdef FILE_SYSTEM_DIRECTORY_ENABLE
            
            memcpy(fname, p_dir->d_name, sizeof(fname));
            sprintf(path3, "%s/%s", RECORD_ROOT_PATH, fname);
            remove(path3);
            ESP_LOGW(TAG, "[file_list_check] delete : %s", path3);

            #endif
		}
    }

	closedir(dir);
    
	return 0;
}

#if 0
/**
 * @brief 保存逆变历史数据
 * 
 * 该函数将保存逆变历史数据到文件中
 * 
 * @param main_node 主节点
 * @param sub_node 子节点
 * @param inv_node 保存该节点的逆变数据指针
 * 
 * @return 成功返回0,否则返回-1
 */
int inv_data_record(int main_node, int sub_node, inv_node_struct *inv_node)
{
	inv_data_record_t inv_record = {0};

	inv_record.node_id = main_node;
	inv_record.inv_id = sub_node;
	inv_record.gird_freq = inv_node->inv_grid.freq;

	memcpy(inv_record.grid_detail, inv_node->inv_grid.grid_detail, sizeof(inv_record.inv_detail));
	memcpy(inv_record.inv_ac_load, inv_node->inv_load.ac_load, sizeof(inv_record.inv_ac_load));

	for(int i = 0; i < sizeof(inv_record.inv_detail)/sizeof(inv_record.inv_detail[0]); i++)
	{
		inv_record.inv_detail[i].power = inv_node->inv_data.inv_detail[i].power;
		inv_record.inv_detail[i].voltage = inv_node->inv_data.inv_detail[i].voltage;
		inv_record.inv_detail[i].current = inv_node->inv_data.inv_detail[i].current;
	}

	for(int i = 0; i < sizeof(inv_record.pv_detail)/sizeof(inv_record.pv_detail[0]); i++)
	{
		inv_record.pv_detail[i].input_power = inv_node->inv_pv.pv_detail[i].input_power;
		inv_record.pv_detail[i].input_voltage = inv_node->inv_pv.pv_detail[i].input_voltage;
		inv_record.pv_detail[i].input_current = inv_node->inv_pv.pv_detail[i].input_current;
	}

	char fname[30] = {0};
	INV_FILE_PATH(fname, main_node, sub_node);
	return historic_data_write(fname, &inv_record,0xFFFF,sizeof(inv_record));
}

/**
 * @brief 保存PACK历史数据
 * 
 * 该函数将保存PACK历史数据到文件中
 * 
 * @param main_node 主节点
 * @param sub_node 子节点
 * @param inv_node 保存该节点的PACK数据指针
 * 
 * @return 成功返回0,否则返回-1
 */
int pack_data_record(int main_node, int sub_node, pack_node_struct *pack_node)
{
	pack_data_record_t pack_record = {0};

	pack_record.node_id = main_node;
	pack_record.pack_id = sub_node;
	pack_record.voltage = pack_node->pack_base.total_voltage;
	pack_record.current = pack_node->pack_base.total_current;
	pack_record.soc = pack_node->pack_base.soc;
	pack_record.soh = pack_node->pack_base.soh;
	pack_record.avg_temp = pack_node->pack_base.avg_temp;
	pack_record.min_cell_voltage = pack_node->pack_base.min_cell_voltage;
	pack_record.max_cell_voltage = pack_node->pack_base.max_cell_voltage;
	pack_record.min_cell_temp = pack_node->pack_base.min_temp_value;
	pack_record.max_cell_temp = pack_node->pack_base.max_temp_value;
	pack_record.min_cell_temp_id = pack_node->pack_base.min_temp_index;
	pack_record.max_cell_temp_id = pack_node->pack_base.max_temp_index;
	pack_record.total_cell_cnt = pack_node->pack_base.pack_total_cell;
	pack_record.total_ntc_cnt = pack_node->pack_base.pack_total_ntc;
	pack_record.pack_cycle = pack_node->pack_extend.cycle_count;

	char fname[30] = {0};
	PACK_FILE_PATH(fname, main_node, sub_node);
	return historic_data_write(fname, &pack_record, 0xFFFF, sizeof(pack_record));
}


//EXT_RAM_BSS_ATTR SetData_TypeDef3 gKwh;//存储在SPI flash

/*
外部spi flash数据存储读取
*/
 void Ext_Flash_Get_data(void)
{

//COMMON_FILE_PATH_YYMM(fname,KWH_RECORD_FOLDER_LEVEL1, reals.rtc_time.year, reals.rtc_time.mon);
//filename_list_get(fname);

	
//	int historic_data_read(fsys_file_list[file_nums].name, void *record, uint32_t len, uint32_t file_pos)
		
// 读取基本设置
//	read_file(NOW_PV_ENERGY_PATH,    (uint8_t *)&SetData.dev_info_t.pv_energy, sizeof(now_energy_t)); // 读取300参数
//	read_file(NOW_GRID_ENERGY_PATH,    (uint8_t *)&SetData.dev_info_t.grid_energy, sizeof(now_energy_t)); // 读取300参数
//	read_file(NOW_HOUR_ENERGY_PATH,    (uint8_t *)&SetData.dev_info_t.pv_hour_enengy, sizeof(SetData.dev_info_t.pv_hour_enengy)+sizeof(SetData.dev_info_t.grid_hour_enengy)); // 读取300参数

}

#endif


/*------------------------------------------------------------------------------
 Function: CheckKwhHourResize
 -----------------------------------------------------------------------------*/
/**
  * @brief      检查kwh小时文件长度（只保留当月文件）
  * @param[in]  const char* filename  
                uint32_t file_pos     
  * @param[out] None
  * @return     void
  */
void CheckKwhHourResize(const char* filename, uint32_t file_pos) 
{
    // 使用open以读写模式打开文件，如果文件不存在则退出
    int file = open(filename, O_RDWR);
    if (file < 0) {
        return;
    }

    // 移动到文件末尾
    off_t fileSize = lseek(file, 0, SEEK_END); // 获取当前文件大小

    if (fileSize > file_pos) 
    {
        ESP_LOGE(TAG, "[CheckHisotryLogResize] old file (%s)", filename);
        
        // 关闭并删除旧文件
        close(file);
        remove(filename);

        return;
    }

    // 关闭文件
    close(file);
}




/*
参数flash

return:
0-ok
other-fail
*/
void Parameter_Write(void)// 
{
	char fname[30] = {0};
	uint32_t file_bias =0;
	uint32_t len =0;
//	uint8_t rtn=0xFF;
//	ESP_LOGI(TAG, "Logger_PopAA 	");
	if (reals.SetDataWrFlag.Byte4 != 0)
	{

	
		PARAMETER_FILE_PATH_IOT(fname,0,0);
		if (0 == SetData_file_data_write(fname, (uint8_t *)&SetData, 0, sizeof(SetData))) //ok
		{
//			dump_buf("SetData_data_write：：", (uint8_t *)&SetData, sizeof(SetData));

			 ESP_LOGI(TAG, "SetData File written ok (%s)",fname);

			if((1 == reals.SetDataWrFlag.sBit.bind_sn)
		   )
			{
				ESP_LOGW(TAG, "[12170 BIND] flash save done, clear trigger_BIND, AckCount=3");
				g_self_data.mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_BIND =0;//绑定完成
				reals.trigger_Bind_AckCount=3;//发送次绑定响应帧
			}
			   
			reals.SetDataWrFlag.Byte4 = 0;	 
		}
		else
		{
			ESP_LOGE(TAG, "SetData File written fail (%s)",fname );
		}		
	}

	if (1 == reals.flasWrFlag.sBit.set_data_inv)
	{

	
		PARAMETER_FILE_PATH_INV(fname,0,0);
		
		
		file_bias =abs((uint32_t)&SetData_Can.dev_info_t2.inv_set00 - (uint32_t)&SetData_Can.ArrayData.CfgCharData[0]);;
		len =sizeof(inv_set00_0x1A_struct_mini)+sizeof(inv_set01_0x1B_struct_mini)+sizeof(inv_set02_0x1C_struct_mini)+sizeof(inv_set03_0x1D_struct_mini);
		if ((0 == SetData_file_data_write(fname, (uint8_t *)&SetData_Can.dev_info_t2.inv_set00,file_bias ,len )) //ok
		&&(0 == SetData_file_data_write(fname, (uint8_t *)&SetData_Can.dev_info_t2.valid_inv,0 ,2 )))//flag WR
		{
			// dump_buf("SetData2_data_write：：", (uint8_t *)&SetData_Can, sizeof(SetData_Can));

			 ESP_LOGI(TAG, "SetData2 set_data_inv File written ok (%s)",fname);
			   
			reals.flasWrFlag.sBit.set_data_inv = 0;	 
		}
		else
		{
			ESP_LOGE(TAG, "SetData2 File written fail (%s)",fname );
		}		
	}	
	else if (1 == reals.flasWrFlag.sBit.set_data_pack)
	{

	
		PARAMETER_FILE_PATH_INV(fname,0,0);
		file_bias =abs((uint32_t)&SetData_Can.dev_info_t2.pack_config - (uint32_t)&SetData_Can.ArrayData.CfgCharData[0]);;
		len =sizeof(pack_config_0x55_struct_mini);
		if ((0 == SetData_file_data_write(fname, (uint8_t *)&SetData_Can.dev_info_t2.pack_config,file_bias ,len )) //ok
			&&(0 == SetData_file_data_write(fname, (uint8_t *)&SetData_Can.dev_info_t2.valid_pack,2 ,2 )))//flag WR
		{
			// dump_buf("SetData2_data_write：：", (uint8_t *)&SetData_Can, sizeof(SetData_Can));

			 ESP_LOGI(TAG, "SetData2 set_data_pack File written ok (%s)",fname);
			   
			reals.flasWrFlag.sBit.set_data_pack = 0;	 
		}
		else
		{
			ESP_LOGE(TAG, "SetData2 File written fail (%s)",fname );
		}		
	}	
	/* TOU控制参数存储 */
    if(1 == Relay_File_W_Flag.sBit.relay_data_soc_ctrl){
        PARAMETER_FILE_PATH_RELAY(fname,0,0);
        file_bias = offsetof(Relay_SetData_TypeDef, Relay_info_t.PowerRelay_SmartSet01);
        len = sizeof(MOD_STRUCT_reg19000);

        if ((0 == SetData_file_data_write(fname, (uint8_t *)&RelaySetData.Relay_info_t.PowerRelay_SmartSet01, file_bias ,len )))
        {
            ESP_LOGI(TAG, "SetData_Relay set_data_soc_ctrl File written ok (%s)",fname);
            Relay_File_W_Flag.sBit.relay_data_soc_ctrl = 0;     
        }
        else
        {
            ESP_LOGE(TAG, "SetData_Relay File written fail (%s)",fname );
        }
    }
    if(1 == Relay_File_W_Flag.sBit.relay_data_delay_ctrl){
        PARAMETER_FILE_PATH_RELAY(fname,0,0);
        file_bias = offsetof(Relay_SetData_TypeDef, Relay_info_t.PowerRelay_SmartSet02);
        len = sizeof(MOD_STRUCT_reg19100);

        if ((0 == SetData_file_data_write(fname, (uint8_t *)&RelaySetData.Relay_info_t.PowerRelay_SmartSet02, file_bias ,len )))
        {
            ESP_LOGI(TAG, "SetData_Relay set_data_time_ctrl File written ok (%s)",fname);
            Relay_File_W_Flag.sBit.relay_data_delay_ctrl = 0;
        }
        else
        {
            ESP_LOGE(TAG, "SetData_Relay File written fail (%s)",fname );
        }
    }
	if(1 == Relay_File_W_Flag.sBit.relay_data_plan_ctrl){
        PARAMETER_FILE_PATH_RELAY(fname,0,0);
        file_bias = offsetof(Relay_SetData_TypeDef, Relay_info_t.Backup_power_set);
        len = sizeof(MOD_STRUCT_reg19200);
        
        if ((0 == SetData_file_data_write(fname, (uint8_t *)&RelaySetData.Relay_info_t.Backup_power_set, file_bias ,len )))
        {
            ESP_LOGI(TAG, "SetData_Relay set_data_plan_ctrl File written ok (%s)",fname);
            Relay_File_W_Flag.sBit.relay_data_plan_ctrl = 0;     
        }
        else
        {
            ESP_LOGE(TAG, "SetData_Relay File written fail (%s)",fname );
        }
    }
    if(1 == Relay_File_W_Flag.sBit.relay_data_time_ctrl){
        PARAMETER_FILE_PATH_RELAY(fname,0,0);
        file_bias = offsetof(Relay_SetData_TypeDef, Relay_info_t.PowerRelay_SmartSet03);
        len = sizeof(MOD_STRUCT_reg19300);
        Relay_Ctrl_Factory_Parameter03_Update();
        if ((0 == SetData_file_data_write(fname, (uint8_t *)&RelaySetData.Relay_info_t.PowerRelay_SmartSet03, file_bias ,len )))
        {
            ESP_LOGI(TAG, "SetData_Relay set_data_delay_ctrl File written ok (%s)",fname);
            Relay_File_W_Flag.sBit.relay_data_time_ctrl = 0;
        }
        else
        {
            ESP_LOGE(TAG, "SetData_Relay File written fail (%s)",fname );
        }
    }



	
//return rtn;
}
/*
扫描部分setData参数是否有变化，有变化自动更新SetData参数
*/
 void Flash_Update_Scan(void)
 {
	if(SetData.dev_info_t.Addr_can_self!=reals.Addr_can_self)
		SetData.dev_info_t.Addr_can_self=reals.Addr_can_self;
 }

/*
AC380仅使用内部flash,无板上外部flash和EEPROM
外部spi flash数据存储写入
*/
 void Ext_Flash_Store(void)
{
	char fname[30] = {0};
	uint32_t file_bias =0;
    static uint8_t rewrite_cnt = 0;

	// ESP_LOGI(TAG, "Ext_Flash_Store 	");
	Parameter_Write();
	if(0 == reals.hour_energy_time.su8tm_year)
	{
		/*时间无效*/
		ESP_LOGE(TAG, "File written fail, time error ");
		reals.kwhWrFlag.Byte2=0;
		rewrite_cnt = 0;
		return;
	}

	
	if(1 == reals.kwhWrFlag.sBit.year_DCLoad_Total)
    {
        INV_KWH_PATH(fname,KWH_RECORD_FILE_MARK_DC_LOAD,0,1);
        file_bias = SetData.dev_info_t.iot_kwh_info.kwh_years_num;
        
        if (0 == historic_data_write(fname, (uint8_t *)&reals.energy_DCLoad_Total[KWH_INFO_ALONE], file_bias, sizeof(year_energy_t)))
        {
            ESP_LOGI(TAG, "File written ok (%s)",fname);
            reals.kwhWrFlag.sBit.year_DCLoad_Total=0;
            rewrite_cnt = 0;
        }
        else
        {
            ESP_LOGE(TAG, "File written fail (%s), rewrite(%d)",fname ,rewrite_cnt);
            rewrite_cnt++;
            
            if(rewrite_cnt > 3)//重新写入失败超过五次则放弃本次写入，避免卡死
            {
                reals.kwhWrFlag.sBit.year_DCLoad_Total=0;
                rewrite_cnt = 0;
                ESP_LOGE(TAG, "File written fail (%s), exit",fname);
            }
        }
    }
	else if(1 == reals.kwhWrFlag.sBit.year_ACLoad_Total)
    {
        INV_KWH_PATH(fname,KWH_RECORD_FILE_MARK_AC_LOAD,0,1);
        file_bias = SetData.dev_info_t.iot_kwh_info.kwh_years_num;
        
        if (0 == historic_data_write(fname, (uint8_t *)&reals.energy_ACLoad_Total[KWH_INFO_ALONE], file_bias, sizeof(year_energy_t)))
        {
            ESP_LOGI(TAG, "File written ok (%s)",fname);
            reals.kwhWrFlag.sBit.year_ACLoad_Total=0;
            rewrite_cnt = 0;
        }
        else
        {
            ESP_LOGE(TAG, "File written fail (%s), rewrite(%d)",fname ,rewrite_cnt);
            rewrite_cnt++;
            
            if(rewrite_cnt > 3)//重新写入失败超过五次则放弃本次写入，避免卡死
            {
                reals.kwhWrFlag.sBit.year_ACLoad_Total=0;
                rewrite_cnt = 0;
                ESP_LOGE(TAG, "File written fail (%s), exit",fname);
            }
        }
    }
	else if(1 == reals.kwhWrFlag.sBit.year_Pv_Total)
    {
        INV_KWH_PATH(fname,KWH_RECORD_FILE_MARK_DCPV,0,1);
        file_bias = SetData.dev_info_t.iot_kwh_info.kwh_years_num;
        
        if (0 == historic_data_write(fname, (uint8_t *)&reals.energy_Pv_dc[KWH_INFO_ALONE], file_bias, sizeof(year_energy_t)))
        {
            ESP_LOGI(TAG, "File written ok (%s)",fname);
            reals.kwhWrFlag.sBit.year_Pv_Total=0;
            rewrite_cnt = 0;
        }
        else
        {
            ESP_LOGE(TAG, "File written fail (%s), rewrite(%d)",fname ,rewrite_cnt);
            rewrite_cnt++;
            
            if(rewrite_cnt > 3)//重新写入失败超过五次则放弃本次写入，避免卡死
            {
                reals.kwhWrFlag.sBit.year_Pv_Total=0;
                rewrite_cnt = 0;
                ESP_LOGE(TAG, "File written fail (%s), exit",fname);
            }
        }
    }
	else if(1 == reals.kwhWrFlag.sBit.year_GridChgin_Total)
    {
        INV_KWH_PATH(fname,KWH_RECORD_FILE_MARK_GRID,0,1);
        file_bias = SetData.dev_info_t.iot_kwh_info.kwh_years_num;
        
        if (0 == historic_data_write(fname, (uint8_t *)&reals.energy_GridChgin_Total[KWH_INFO_ALONE], file_bias, sizeof(year_energy_t)))
        {
            ESP_LOGI(TAG, "File written ok (%s)",fname);
            reals.kwhWrFlag.sBit.year_GridChgin_Total=0;
            rewrite_cnt = 0;
        }
        else
        {
            ESP_LOGE(TAG, "File written fail (%s), rewrite(%d)",fname ,rewrite_cnt);
            rewrite_cnt++;
            
            if(rewrite_cnt > 3)//重新写入失败超过五次则放弃本次写入，避免卡死
            {
                reals.kwhWrFlag.sBit.year_GridChgin_Total=0;
                rewrite_cnt = 0;
                ESP_LOGE(TAG, "File written fail (%s), exit",fname);
            }
        }
    }
	else if(1 == reals.kwhWrFlag.sBit.year_GridFeedback_Total)
    {
        INV_KWH_PATH(fname,KWH_RECORD_FILE_MARK_FD,0,1);
        file_bias = SetData.dev_info_t.iot_kwh_info.kwh_years_num;
        
        if (0 == historic_data_write(fname, (uint8_t *)&reals.energy_GridFeedback_Total[KWH_INFO_ALONE], file_bias, sizeof(year_energy_t)))
        {
            ESP_LOGI(TAG, "File written ok (%s)",fname);
            reals.kwhWrFlag.sBit.year_GridFeedback_Total=0;
            rewrite_cnt = 0;
        }
        else
        {
            ESP_LOGE(TAG, "File written fail (%s), rewrite(%d)",fname ,rewrite_cnt);
            rewrite_cnt++;
            
            if(rewrite_cnt > 3)//重新写入失败超过五次则放弃本次写入，避免卡死
            {
                reals.kwhWrFlag.sBit.year_GridFeedback_Total=0;
                rewrite_cnt = 0;
                ESP_LOGE(TAG, "File written fail (%s), exit",fname);
            }
        }
    }
	else if(1 == reals.kwhWrFlag.sBit.year_PVToload_Total)
    {
        INV_KWH_PATH(fname,KWH_RECORD_FILE_MARK_PV_TO_LOAD,0,1);

        file_bias = SetData.dev_info_t.iot_kwh_info.kwh_years_num;
        
        if (0 == historic_data_write(fname, (uint8_t *)&reals.energy_PVToload_Total[KWH_INFO_ALONE], file_bias, sizeof(year_energy_t)))
        {
            ESP_LOGI(TAG, "File written ok (%s)",fname);
            reals.kwhWrFlag.sBit.year_PVToload_Total=0;
            rewrite_cnt = 0;
        }
        else
        {
            ESP_LOGE(TAG, "File written fail (%s), rewrite(%d)",fname ,rewrite_cnt);
            rewrite_cnt++;
            
            if(rewrite_cnt > 3)//重新写入失败超过五次则放弃本次写入，避免卡死
            {
                reals.kwhWrFlag.sBit.year_PVToload_Total=0;
                rewrite_cnt = 0;
                ESP_LOGE(TAG, "File written fail (%s), exit",fname);
            }
        }
    }
	else if(1 == reals.kwhWrFlag.sBit.year_PackDsg_Total)
    {
        INV_KWH_PATH(fname,KWH_RECORD_FILE_MARK_BATDICH,0,1);

        file_bias = SetData.dev_info_t.iot_kwh_info.kwh_years_num;
        
        if (0 == historic_data_write(fname, (uint8_t *)&reals.energy_PackDsg_Total[KWH_INFO_ALONE], file_bias, sizeof(year_energy_t)))
        {
            ESP_LOGI(TAG, "File written ok (%s)",fname);
            reals.kwhWrFlag.sBit.year_PackDsg_Total=0;
            rewrite_cnt = 0;
        }
        else
        {
            ESP_LOGE(TAG, "File written fail (%s), rewrite(%d)",fname ,rewrite_cnt);
            rewrite_cnt++;
            
            if(rewrite_cnt > 3)//重新写入失败超过五次则放弃本次写入，避免卡死
            {
                reals.kwhWrFlag.sBit.year_PackDsg_Total=0;
                rewrite_cnt = 0;
                ESP_LOGE(TAG, "File written fail (%s), exit",fname);
            }
        }
    }
    else if(1 == reals.kwhWrFlag.sBit.hour_file)
    {
        /*受限于flash，只存储当月的电量小时文件，文件名不作年月及电量类型区分*/
        INV_KWH_PATH(fname,KWH_RECORD_FILE_MARK_HOUR,0,1);

        file_bias = (reals.hour_energy_time.su8tm_mday -1)*24 + reals.hour_energy_time.su8tm_hour;//24*31
		//ESP_LOGI(TAG, " 1 File written hour bias:%d mday:%d hour:%d",(unsigned int)file_bias,(unsigned int)reals.hour_energy_time.su8tm_mday,(unsigned int)reals.hour_energy_time.su8tm_hour);
        //ESP_LOGI(TAG, " 1 File written hour DCLoad_Total:%d Pv_dc:%d ",(unsigned int)reals.hour_energy[KWH_INFO_ALONE].DCLoad_Total,(unsigned int)reals.hour_energy[KWH_INFO_ALONE].Pv_dc);
		CheckKwhHourResize(fname, (file_bias*sizeof(baseKWH_energy_t_u16)));
        
        if (0 == historic_data_write(fname, (uint8_t *)&reals.hour_energy[KWH_INFO_ALONE], file_bias, sizeof(baseKWH_energy_t_u16))) 
        {

             ESP_LOGI(TAG, "File written ok (%s)",fname);
			 reals.kwhWrFlag.sBit.hour_file = 0;
             rewrite_cnt = 0;
        }
        else
        {
            ESP_LOGE(TAG, "File written fail (%s), rewrite(%d)",fname ,rewrite_cnt);
            rewrite_cnt++;

            if(rewrite_cnt > 3)//重新写入失败超过五次则放弃本次写入，避免卡死
            {
				reals.kwhWrFlag.sBit.hour_file = 0;
                rewrite_cnt = 0;
                ESP_LOGE(TAG, "File written fail (%s), exit",fname);
            }
        }
    }
    else if(1 == reals.flasWrFlag.sBit.log_fault)
    {
		Inv_Fault_Log_Pop();
		reals.flasWrFlag.sBit.log_fault =0;
    }
    else if(1 == reals.flasWrFlag.sBit.log_event)
    {
		Logger_Pop_Event();
		reals.flasWrFlag.sBit.log_event =0;
    }
	else if(1 == reals.flasWrFlag.sBit.log_invdetailedinfo )
	{
		Inv_Detailed_Info_Log_Pop();
	}		
	else if(1 == reals.kwhWrFlag.sBit.year_DCLoad_Total_Sum)
    {
        INV_KWH_PATH(fname,KWH_RECORD_FILE_MARK_DC_LOAD,0,0);
        file_bias = SetData.dev_info_t.iot_kwh_info.kwh_years_num;
        
        if (0 == historic_data_write(fname, (uint8_t *)&reals.energy_DCLoad_Total[KWH_INFO_SUM], file_bias, sizeof(year_energy_t)))
        {
            ESP_LOGI(TAG, "File written ok (%s)",fname);
            reals.kwhWrFlag.sBit.year_DCLoad_Total_Sum=0;
            rewrite_cnt = 0;
        }
        else
        {
            ESP_LOGE(TAG, "File written fail (%s), rewrite(%d)",fname ,rewrite_cnt);
            rewrite_cnt++;
            
            if(rewrite_cnt > 3)//重新写入失败超过五次则放弃本次写入，避免卡死
            {
                reals.kwhWrFlag.sBit.year_DCLoad_Total_Sum=0;
                rewrite_cnt = 0;
                ESP_LOGE(TAG, "File written fail (%s), exit",fname);
            }
        }
    }
	else if(1 == reals.kwhWrFlag.sBit.year_ACLoad_Total_Sum)
    {
        INV_KWH_PATH(fname,KWH_RECORD_FILE_MARK_AC_LOAD,0,0);
        file_bias = SetData.dev_info_t.iot_kwh_info.kwh_years_num;
        
        if (0 == historic_data_write(fname, (uint8_t *)&reals.energy_ACLoad_Total[KWH_INFO_SUM], file_bias, sizeof(year_energy_t)))
        {
            ESP_LOGI(TAG, "File written ok (%s)",fname);
            reals.kwhWrFlag.sBit.year_ACLoad_Total_Sum=0;
            rewrite_cnt = 0;
        }
        else
        {
            ESP_LOGE(TAG, "File written fail (%s), rewrite(%d)",fname ,rewrite_cnt);
            rewrite_cnt++;
            
            if(rewrite_cnt > 3)//重新写入失败超过五次则放弃本次写入，避免卡死
            {
                reals.kwhWrFlag.sBit.year_ACLoad_Total_Sum=0;
                rewrite_cnt = 0;
                ESP_LOGE(TAG, "File written fail (%s), exit",fname);
            }
        }
    }
	else if(1 == reals.kwhWrFlag.sBit.year_Pv_Total_Sum)
    {
        INV_KWH_PATH(fname,KWH_RECORD_FILE_MARK_DCPV,0,0);
        file_bias = SetData.dev_info_t.iot_kwh_info.kwh_years_num;
        
        if (0 == historic_data_write(fname, (uint8_t *)&reals.energy_Pv_dc[KWH_INFO_SUM], file_bias, sizeof(year_energy_t)))
        {
            ESP_LOGI(TAG, "File written ok (%s)",fname);
            reals.kwhWrFlag.sBit.year_Pv_Total_Sum=0;
            rewrite_cnt = 0;
        }
        else
        {
            ESP_LOGE(TAG, "File written fail (%s), rewrite(%d)",fname ,rewrite_cnt);
            rewrite_cnt++;
            
            if(rewrite_cnt > 3)//重新写入失败超过五次则放弃本次写入，避免卡死
            {
                reals.kwhWrFlag.sBit.year_Pv_Total_Sum=0;
                rewrite_cnt = 0;
                ESP_LOGE(TAG, "File written fail (%s), exit",fname);
            }
        }
    }
	else if(1 == reals.kwhWrFlag.sBit.year_GridChgin_Total_Sum)
    {
        INV_KWH_PATH(fname,KWH_RECORD_FILE_MARK_GRID,0,0);
        file_bias = SetData.dev_info_t.iot_kwh_info.kwh_years_num;
        
        if (0 == historic_data_write(fname, (uint8_t *)&reals.energy_GridChgin_Total[KWH_INFO_SUM], file_bias, sizeof(year_energy_t)))
        {
            ESP_LOGI(TAG, "File written ok (%s)",fname);
            reals.kwhWrFlag.sBit.year_GridChgin_Total_Sum=0;
            rewrite_cnt = 0;
        }
        else
        {
            ESP_LOGE(TAG, "File written fail (%s), rewrite(%d)",fname ,rewrite_cnt);
            rewrite_cnt++;
            
            if(rewrite_cnt > 3)//重新写入失败超过五次则放弃本次写入，避免卡死
            {
                reals.kwhWrFlag.sBit.year_GridChgin_Total_Sum=0;
                rewrite_cnt = 0;
                ESP_LOGE(TAG, "File written fail (%s), exit",fname);
            }
        }
    }
	else if(1 == reals.kwhWrFlag.sBit.year_GridFeedback_Total_Sum)
    {
        INV_KWH_PATH(fname,KWH_RECORD_FILE_MARK_FD,0,0);
        file_bias = SetData.dev_info_t.iot_kwh_info.kwh_years_num;
        
        if (0 == historic_data_write(fname, (uint8_t *)&reals.energy_GridFeedback_Total[KWH_INFO_SUM], file_bias, sizeof(year_energy_t)))
        {
            ESP_LOGI(TAG, "File written ok (%s)",fname);
            reals.kwhWrFlag.sBit.year_GridFeedback_Total_Sum=0;
            rewrite_cnt = 0;
        }
        else
        {
            ESP_LOGE(TAG, "File written fail (%s), rewrite(%d)",fname ,rewrite_cnt);
            rewrite_cnt++;
            
            if(rewrite_cnt > 3)//重新写入失败超过五次则放弃本次写入，避免卡死
            {
                reals.kwhWrFlag.sBit.year_GridFeedback_Total_Sum=0;
                rewrite_cnt = 0;
                ESP_LOGE(TAG, "File written fail (%s), exit",fname);
            }
        }
    }
	else if(1 == reals.kwhWrFlag.sBit.year_PVToload_Total_Sum)
    {
        INV_KWH_PATH(fname,KWH_RECORD_FILE_MARK_PV_TO_LOAD,0,0);

        file_bias = SetData.dev_info_t.iot_kwh_info.kwh_years_num;
        
        if (0 == historic_data_write(fname, (uint8_t *)&reals.energy_PVToload_Total[KWH_INFO_SUM], file_bias, sizeof(year_energy_t)))
        {
            ESP_LOGI(TAG, "File written ok (%s)",fname);
            reals.kwhWrFlag.sBit.year_PVToload_Total_Sum=0;
            rewrite_cnt = 0;
        }
        else
        {
            ESP_LOGE(TAG, "File written fail (%s), rewrite(%d)",fname ,rewrite_cnt);
            rewrite_cnt++;
            
            if(rewrite_cnt > 3)//重新写入失败超过五次则放弃本次写入，避免卡死
            {
                reals.kwhWrFlag.sBit.year_PVToload_Total_Sum=0;
                rewrite_cnt = 0;
                ESP_LOGE(TAG, "File written fail (%s), exit",fname);
            }
        }
    }
	else if(1 == reals.kwhWrFlag.sBit.year_PackDsg_Total_Sum)
    {
        INV_KWH_PATH(fname,KWH_RECORD_FILE_MARK_BATDICH,0,0);

        file_bias = SetData.dev_info_t.iot_kwh_info.kwh_years_num;
        
        if (0 == historic_data_write(fname, (uint8_t *)&reals.energy_PackDsg_Total[KWH_INFO_SUM], file_bias, sizeof(year_energy_t)))
        {
            ESP_LOGI(TAG, "File written ok (%s)",fname);
            reals.kwhWrFlag.sBit.year_PackDsg_Total_Sum=0;
            rewrite_cnt = 0;
        }
        else
        {
            ESP_LOGE(TAG, "File written fail (%s), rewrite(%d)",fname ,rewrite_cnt);
            rewrite_cnt++;
            
            if(rewrite_cnt > 3)//重新写入失败超过五次则放弃本次写入，避免卡死
            {
                reals.kwhWrFlag.sBit.year_PackDsg_Total_Sum=0;
                rewrite_cnt = 0;
                ESP_LOGE(TAG, "File written fail (%s), exit",fname);
            }
        }
    }
    else if(1 == reals.kwhWrFlag.sBit.hour_file_Sum)
    {
        /*受限于flash，只存储当月的电量小时文件，文件名不作年月及电量类型区分*/
        INV_KWH_PATH(fname,KWH_RECORD_FILE_MARK_HOUR,0,0);
        
        file_bias = (reals.hour_energy_time.su8tm_mday -1)*24 + reals.hour_energy_time.su8tm_hour;//24*31
		ESP_LOGI(TAG, " 0 File written hour bias:%d mday:%d hour:%d",(unsigned int)file_bias,(unsigned int)reals.hour_energy_time.su8tm_mday,(unsigned int)reals.hour_energy_time.su8tm_hour);
		ESP_LOGI(TAG, " 0 File written hour DCLoad_Total:%d Pv_dc:%d ",(unsigned int)reals.hour_energy[KWH_INFO_SUM].DCLoad_Total,(unsigned int)reals.hour_energy[KWH_INFO_SUM].Pv_dc);
	    CheckKwhHourResize(fname, (file_bias*sizeof(baseKWH_energy_t_u16)));
        
        if (0 == historic_data_write(fname, (uint8_t *)&reals.hour_energy[KWH_INFO_SUM], file_bias, sizeof(baseKWH_energy_t_u16))) 
        {

             ESP_LOGI(TAG, "File written ok (%s)",fname);
			 reals.kwhWrFlag.sBit.hour_file_Sum = 0;
             rewrite_cnt = 0;
        }
        else
        {
            ESP_LOGE(TAG, "File written fail (%s), rewrite(%d)",fname ,rewrite_cnt);
            rewrite_cnt++;

            if(rewrite_cnt > 3)//重新写入失败超过五次则放弃本次写入，避免卡死
            {
				reals.kwhWrFlag.sBit.hour_file_Sum = 0;
                rewrite_cnt = 0;
                ESP_LOGE(TAG, "File written fail (%s), exit",fname);
            }
        }
    }	


	
//    else
//    {
//        return;//未发生写入，直接退出
//    }

}
 
 
  /*------------------------------------------------------------------------------
  Function: get_parameter_from_flash
  -----------------------------------------------------------------------------*/
 /**
   * @brief 	  
   * @param[in]  void  
   * @param[out] None
   * @return	 static void
   */
  int get_parameter_from_flash(void)
 {
	 char path[50] = {0};
	 char fname[30] = {0};
	 
	 int fd;
 
	  PARAMETER_FILE_PATH_IOT(fname,0,0);
	  sprintf(path, "%s/%s", RECORD_ROOT_PATH, fname);
  
	  historic_dir_check(fname);
  
	  /* 只读格式打开文件 */
	  fd = open(path, O_RDONLY);
	  if(fd < 0)
	  {
		  ESP_LOGE(TAG, "[iot file para SetData] open error");
		  /*读取有误，恢复为默认值*/
		  Default_iot_data_init();
		  
		 if (0 == SetData_file_data_write(fname, (uint8_t *)&SetData, 0, sizeof(SetData))) //ok
		 {
//			 dump_buf("SetData_data_write：：", (uint8_t *)&SetData, sizeof(SetData));
 
			  ESP_LOGI(TAG, "SetData File written ok (%s)",fname);
 
		 }
 // 	 //////////
 // 	  fd = open(path, O_RDONLY);
 // 	  if(fd < 0)
 // 	  {
 // 		  ESP_LOGE(TAG, "[iot file para SetData] open error2222");
 //
 // 	  }
 // 	  return -1;
	  }
	 else
	 {
		 /* 移动文件指针到设置的数据位置 */
			 if(lseek(fd, 0, SEEK_SET) != 0) 
			 {
				 ESP_LOGE(TAG, "[iot file para SetData] start_pos set error");
				 goto __error;
			 }
			 
			 /* 读数据到缓存 */
			 if(read(fd, &SetData, sizeof(SetData)) != sizeof(SetData)) //fail
			 {
				 ESP_LOGE(TAG, "[iot file para SetData] read error");
				 goto __error;
			 }
			 else//ok
			 {
				 /*判断读取标志位是否合法*/
				 if(IOT1EPROM_READY_FLAG != SetData.dev_info_t.valid_iot)//参数初始化
				 {
					 /*读取有误，恢复为默认值*/
					 Default_iot_data_init();
					 reals.SetDataWrFlag.Byte4=0xFFFFFFFF;
				 }	 
		 
			 }
			 close(fd);
 
	 }
	 
	 return 0;
 
 __error:
	 close(fd);
	 return -1;

 }
 
  
  
   /*------------------------------------------------------------------------------
   Function: get_parameter_from_flash
   -----------------------------------------------------------------------------*/
  /**
	* @brief	   
	* @param[in]  void	
	* @param[out] None
	* @return	  static void
	*/
int get_parameter_from_flash_2(void)
{
  char path[50] = {0};
  char fname[30] = {0};
  
  int fd;

   PARAMETER_FILE_PATH_INV(fname,0,0);
   sprintf(path, "%s/%s", RECORD_ROOT_PATH, fname);

   historic_dir_check(fname);

   /* 只读格式打开文件 */
   fd = open(path, O_RDONLY);
   if(fd < 0)
   {
	   ESP_LOGE(TAG, "[iot file para SetData2] open error");
	   /*读取有误，恢复为默认值*/
//	   Default_iot_data_init();
	   memset((uint8_t *)&SetData_Can, 0, sizeof(SetData_Can));
	   
	  if (0 == SetData_file_data_write(fname, (uint8_t *)&SetData_Can, 0, sizeof(SetData_Can))) //ok
	  {
		//   dump_buf("SetData2_data_write：：", (uint8_t *)&SetData_Can, sizeof(SetData_Can));

		   ESP_LOGI(TAG, "SetData2 File written ok (%s)",fname);

	  }
//	  //////////
//	   fd = open(path, O_RDONLY);
//	   if(fd < 0)
//	   {
//		   ESP_LOGE(TAG, "[iot file para SetData] open error2222");
//
//	   }
//	   return -1;
   }
  else
  {
	  /* 移动文件指针到设置的数据位置 */
		  if(lseek(fd, 0, SEEK_SET) != 0) 
		  {
			  ESP_LOGE(TAG, "[iot file para SetData2] start_pos set error");
			  goto __error;
		  }
		  
		  /* 读数据到缓存 */
		  if(read(fd, &SetData_Can, sizeof(SetData_Can)) != sizeof(SetData_Can)) //fail
		  {
			  ESP_LOGE(TAG, "[iot file para SetData2] read error");
			  goto __error;
		  }
		  else//ok
		  {
			  /*判断读取标志位是否合法*/
			  if(INV_EPROM_READY_FLAG != SetData_Can.dev_info_t2.valid_inv)//参数初始化
			  {
//					  /*读取有误，恢复为默认值*/
//					  Default_iot_data_init();
//					  reals.SetDataWrFlag.Byte4=0xFFFFFFFF;
			  }   
	  
		  }
		  close(fd);

  }
  
  return 0;

__error:
  close(fd);
  return -1;

}




 /*
从文件系统中，基于指定的路径读取KWH参数，存储到对应的结构体变量

 */
 int InitIotKwhFile(char *RecordName,void *_target_energy)
 {
	 uint32_t file_pos = 0;
	 char path[30] = {0};
	 int fd;
	 uint16_t energy_length = sizeof(year_energy_t);
	 year_energy_t* target_energy = (year_energy_t*)_target_energy;
 
	 sprintf(path, "%s/%s%d_%02d", RECORD_ROOT_PATH, RecordName, 0, 0);//K/pv
 
	 /* 只读格式打开文件 */
	 fd = open(path, O_RDONLY);
	 if(fd < 0)
	 {
		 ESP_LOGE(TAG, "[GetInitDataFromLittlefs] open error(%s)",path);
		 return -1;
	 }
 
	 /* 移动文件指针到设置的数据位置 */
	 file_pos = SetData.dev_info_t.iot_kwh_info.kwh_years_num * energy_length;//reals.pv_energy
	 // lseek(fd, file_pos, SEEK_SET)
	 if(lseek(fd, file_pos, SEEK_SET) != file_pos) 
	 {
		 ESP_LOGE(TAG, "[GetInitDataFromLittlefs] start_pos set error(%s)",path);
		 goto __error;
	 }
 
	 /* 读数据到缓存 */
	 if(read(fd, target_energy, energy_length) != energy_length) //&reals.pv_energy
	 {
		 ESP_LOGE(TAG, "[GetInitDataFromLittlefs] read error(%s)",path);
		 goto __error;
	 }
 
	 close(fd);
	 ESP_LOGI(TAG, "[GetInitDataFromLittlefs] %s init ok(%s)",RecordName,path);
	 return 0;
 
 __error:
   close(fd);
   return -1;
 
 }

/*------------------------------------------------------------------------------
Function: clear_kwh_file
-----------------------------------------------------------------------------*/
/**
* @brief	 清除所有能量记录文件  
* @param[in]  void	
* @param[out] None
* @return	  static void
*/
int clear_kwh_file(void)
{
  char path[50] = {0};
  char fname[30] = {0};
  
  int fd;

  /////////////////////log_history
//   LOG_FILE_PATH_INV(fname);
  
	for(uint8_t node=0;node<2;node++)
	{
		for(uint8_t i = PV_ENERGY_TYPE ; i < ENERGY_TYPE_MAX ; i++)
		{
			//sprintf(path, "%s/%s%s%d_00", RECORD_ROOT_PATH, LOG_RECORD_FOLDER_LEVEL1, PARAMETER_FILE_MARK_INV, i); 
			if (i == PV_ENERGY_TYPE)
			{
				sprintf(path, "%s/%s%d_%02d", RECORD_ROOT_PATH, KWH_RECORD_FILE_MARK_DCPV, 0, node);
			}
			else if(i == GRID_ENERGY_INPUT_TYPE)
			{
				sprintf(path, "%s/%s%d_%02d", RECORD_ROOT_PATH, KWH_RECORD_FILE_MARK_GRID, 0, node);
			}
			else if(i == GRID_ENERGY_OUTPUT_TYPE)
			{
				sprintf(path, "%s/%s%d_%02d", RECORD_ROOT_PATH, KWH_RECORD_FILE_MARK_FD, 0, node);
			}
			else if(i == AC_LOAD_ENERGY_TYPE)
			{
				sprintf(path, "%s/%s%d_%02d", RECORD_ROOT_PATH, KWH_RECORD_FILE_MARK_AC_LOAD, 0, node);
			}
			else if(i == DC_LOAD_ENERGY_TYPE)
			{
				sprintf(path, "%s/%s%d_%02d", RECORD_ROOT_PATH, KWH_RECORD_FILE_MARK_DC_LOAD, 0, node);
			}
			else if(i == AC_PV_ENERGY_TYPE)
			{
				sprintf(path, "%s/%s%d_%02d", RECORD_ROOT_PATH, KWH_RECORD_FILE_MARK_ACPV, 0, node);
			}
			else if(i == DC_PV_ENERGY_TYPE)
			{
				sprintf(path, "%s/%s%d_%02d", RECORD_ROOT_PATH, KWH_RECORD_FILE_MARK_DCPV, 0, node);
			}
			else if(i == BAT_TOTAL_CHARGE_ENERGY_TYPE)
			{
				sprintf(path, "%s/%s%d_%02d", RECORD_ROOT_PATH, KWH_RECORD_FILE_MARK_BATDICH, 0, node);
			}
			else if(i == BAT_TOTAL_DISCHARGE_ENERGY_TYPE)
			{
				sprintf(path, "%s/%s%d_%02d", RECORD_ROOT_PATH, KWH_RECORD_FILE_MARK_BATDICH, 0, node);
			}
			else if(i == PV_TO_ACLOAD_ENERGY_TYPE)
			{
				sprintf(path, "%s/%s%d_%02d", RECORD_ROOT_PATH, KWH_RECORD_FILE_MARK_PV_TO_LOAD, 0, node);
			}
			else//无效类型
			{
				return -1;
			}
				
			ESP_LOGE(TAG, "clear KWH file name: %s", path);

			/* 只读格式打开文件 */
			fd = open(path, O_RDONLY);
			if(fd < 0)
			{
				ESP_LOGE(TAG, "[log_file] open error, %s", path);
				continue;
			}

			close(fd);
			remove(path);
			ESP_LOGI(TAG, "clear %s log_history ok", path);
		}
		sprintf(path, "%s/%s%d_%02d", RECORD_ROOT_PATH, KWH_RECORD_FILE_MARK_HOUR, 0, node);

		ESP_LOGE(TAG, "clear KWH hour file name: %s", path);
		/* 只读格式打开文件 */
		fd = open(path, O_RDONLY);
		if(fd < 0)
		{
			ESP_LOGE(TAG, "[log_file] open error, %s", path);
			continue;
		}

		close(fd);
		remove(path);
		ESP_LOGI(TAG, "clear %s log_history ok", path);

	}
  
  return 0;
}

/*------------------------------------------------------------------------------
Function: clear_log_file
-----------------------------------------------------------------------------*/
/**
* @brief	   
* @param[in]  void	
* @param[out] None
* @return	  static void
*/
int clear_log_file(void)
{
  char path[50] = {0};
  char fname[30] = {0};
  
  int fd;

  /////////////////////log_history
//   LOG_FILE_PATH_INV(fname);
  
	for(uint8_t i = 0 ; i < DEV_MAIN_NODE_MAX ; i++)
	{
		sprintf(path, "%s/%s%s%d_00", RECORD_ROOT_PATH, LOG_RECORD_FOLDER_LEVEL1, PARAMETER_FILE_MARK_INV, i); 
		ESP_LOGE(TAG, "clear log_history file name: %s", path);

		/* 只读格式打开文件 */
		fd = open(path, O_RDONLY);
		if(fd < 0)
		{
			ESP_LOGE(TAG, "[log_file] open error, %s", path);
			continue;
		}

		close(fd);
		remove(path);
		ESP_LOGI(TAG, "clear %s log_history ok", path);
	}
	sprintf(path, "%s/%s%s15_00", RECORD_ROOT_PATH, LOG_RECORD_FOLDER_LEVEL1, PARAMETER_FILE_MARK_INV); //清除汇总文件
	ESP_LOGE(TAG, "clear log_history file name: %s", path);
	/* 只读格式打开文件 */
	fd = open(path, O_RDONLY);
	if(fd < 0)
	{
		ESP_LOGE(TAG, "[log_file] open error, %s", path);
		return -1;
	}

	close(fd);
	remove(path);
	ESP_LOGI(TAG, "clear %s log_history ok", path);

  
  return 0;
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
  Function: Copy_Data_From_Set_To_Modbus
  -----------------------------------------------------------------------------*/
 /**
   * @brief 	 更新本地参数至modbus寄存器
   * @param[in]  void  
   * @param[out] None
   * @return	 static void
   */
  void Copy_Data_From_Set_To_Modbus(void)
 {
    g_self_data.mod_reg00000.iot_state.bits.support_lcd = 1; //支持慈吸屏
    // g_self_data.mod_reg00000.iot_state.bits.support_shelly = 1; //支持shelly电表
#ifdef CONFIG_MODBUS_REG_TLV_ENABLE
	g_self_data.mod_reg00000.iot_state.bits.modbus_tlv_enable = 1; //支持ModbusTLV（功能码40004/40005）
#endif
	 memcpy(login2_info.raw_url, SetData.dev_info_t.Net_Server_address, sizeof(SetData.dev_info_t.Net_Server_address));
	 memcpy(g_self_data.mod_reg00000.app_password, SetData.dev_info_t.app_password,  sizeof(SetData.dev_info_t.app_password));
	 memcpy(g_self_data.mod_reg12000_IOT_set.wifi_sta_ssid, SetData.dev_info_t.wifi_sta_ssid,  sizeof(SetData.dev_info_t.wifi_sta_ssid));
	 memcpy(g_self_data.mod_reg12000_IOT_set.wifi_sta_password, SetData.dev_info_t.wifi_sta_password,  sizeof(SetData.dev_info_t.wifi_sta_password));
	 memcpy(&g_self_data.mod_reg12000_IOT_set.thunder_ctrl.all, &SetData.dev_info_t.thunder_ctrl.all,  sizeof(SetData.dev_info_t.thunder_ctrl.all));
	 memcpy(&g_self_data.mod_reg12000_IOT_set.on_off, &SetData.dev_info_t.on_off,  sizeof(SetData.dev_info_t.on_off));
	 memcpy(g_self_data.mod_reg12000_IOT_set.could_dns, SetData.dev_info_t.could_dns,  sizeof(SetData.dev_info_t.could_dns));
	 SetData.dev_info_t.sta_enable.open_wifi_support = 1; /* 固件能力位：支持开放式WiFi */
	 memcpy(&g_self_data.mod_reg12000_IOT_set.sta_enable, &SetData.dev_info_t.sta_enable,  sizeof(SetData.dev_info_t.sta_enable));
	 memcpy(&g_self_data.mod_reg12000_IOT_set.wifi_sta_auth, &SetData.dev_info_t.wifi_sta_auth,  sizeof(SetData.dev_info_t.wifi_sta_auth));
	 memcpy(g_self_data.mod_reg22000_net_server_2rd.Net_Server_address, SetData.dev_info_t.Net_Server_address,	sizeof(SetData.dev_info_t.Net_Server_address));
	 memcpy(g_self_data.mod_reg22000_net_server_2rd.Net_Server_secret, SetData.dev_info_t.Net_Server_secret,  sizeof(SetData.dev_info_t.Net_Server_secret));
 
     g_self_data.mod_reg13600_open.ble_protocol = SetData.dev_info_t.ble_protocol;
     g_self_data.mod_reg13600_open.blec_rssi_threshold = SetData.dev_info_t.blec_rssi_th;
     g_self_data.mod_reg13600_open.blec_switch_interval = SetData.dev_info_t.blec_switch_int;
     memcpy(g_self_data.mod_reg13600_open.bles_adv_key, SetData.dev_info_t.bles_adv_key, sizeof(SetData.dev_info_t.bles_adv_key));

     g_self_data.mod_reg13600_open.wifi_sta_rssi_threshold = SetData.dev_info_t.wifi_sta_rssi_th;
     g_self_data.mod_reg13600_open.wifi_sta_switch_interval = SetData.dev_info_t.wifi_sta_switch_int;
     g_self_data.mod_reg13600_open.wifi_mul_sta_en = SetData.dev_info_t.wifi_mul_sta_en;
     g_self_data.mod_reg13600_open.wifi_sta1_ip = SetData.dev_info_t.wifi_sta1_ip;
     g_self_data.mod_reg13600_open.wifi_sta1_mask = SetData.dev_info_t.wifi_sta1_mask;
     g_self_data.mod_reg13600_open.wifi_sta1_gw = SetData.dev_info_t.wifi_sta1_gw;
     g_self_data.mod_reg13600_open.wifi_sta1_dns1 = SetData.dev_info_t.wifi_sta1_dns1;
     g_self_data.mod_reg13600_open.wifi_sta1_dns2 = SetData.dev_info_t.wifi_sta1_dns2;
     g_self_data.mod_reg13600_open.wifi_sta2_auth = SetData.dev_info_t.wifi_sta2_auth;
     memcpy(g_self_data.mod_reg13600_open.wifi_sta2_ssid, SetData.dev_info_t.wifi_sta2_ssid, sizeof(SetData.dev_info_t.wifi_sta2_ssid));
	 memcpy(g_self_data.mod_reg13600_open.wifi_sta2_password, SetData.dev_info_t.wifi_sta2_password, sizeof(SetData.dev_info_t.wifi_sta2_password));
     g_self_data.mod_reg13600_open.wifi_sta2_ip = SetData.dev_info_t.wifi_sta2_ip;
     g_self_data.mod_reg13600_open.wifi_sta2_mask = SetData.dev_info_t.wifi_sta2_mask;
     g_self_data.mod_reg13600_open.wifi_sta2_gw = SetData.dev_info_t.wifi_sta2_gw;
     g_self_data.mod_reg13600_open.wifi_sta2_dns1 = SetData.dev_info_t.wifi_sta2_dns1;
     g_self_data.mod_reg13600_open.wifi_sta2_dns2 = SetData.dev_info_t.wifi_sta2_dns2;
     g_self_data.mod_reg13600_open.wifi_sta3_auth = SetData.dev_info_t.wifi_sta3_auth;
     memcpy(g_self_data.mod_reg13600_open.wifi_sta3_ssid, SetData.dev_info_t.wifi_sta3_ssid, sizeof(SetData.dev_info_t.wifi_sta3_ssid));
     memcpy(g_self_data.mod_reg13600_open.wifi_sta3_password, SetData.dev_info_t.wifi_sta3_password, sizeof(SetData.dev_info_t.wifi_sta3_password));
     g_self_data.mod_reg13600_open.wifi_sta3_ip = SetData.dev_info_t.wifi_sta3_ip;
     g_self_data.mod_reg13600_open.wifi_sta3_mask = SetData.dev_info_t.wifi_sta3_mask;
     g_self_data.mod_reg13600_open.wifi_sta3_gw = SetData.dev_info_t.wifi_sta3_gw;
     g_self_data.mod_reg13600_open.wifi_sta3_dns1 = SetData.dev_info_t.wifi_sta3_dns1;
     g_self_data.mod_reg13600_open.wifi_sta3_dns2 = SetData.dev_info_t.wifi_sta3_dns2;

     g_self_data.mod_reg12000_IOT_set.wifi_ap_auth = SetData.dev_info_t.wifi_sta3_auth;
     memcpy(g_self_data.mod_reg12000_IOT_set.wifi_AP_ssid, SetData.dev_info_t.wifi_ap_ssid, sizeof(SetData.dev_info_t.wifi_ap_ssid));
     memcpy(g_self_data.mod_reg12000_IOT_set.wifi_AP_password, SetData.dev_info_t.wifi_ap_password, sizeof(SetData.dev_info_t.wifi_ap_password));

     g_self_data.mod_reg13600_open.open_mqtt_enable = SetData.dev_info_t.open_mqtt_enable;
     g_self_data.mod_reg13600_open.open_mqtt_report_cycle = SetData.dev_info_t.open_mqtt_report_cycle;

    g_self_data.mod_reg13600_open.modbus_tcp_enable.all = SetData.dev_info_t.modbus_tcp_enable.all;
    g_self_data.mod_reg13600_open.modbus_tcp_port = SetData.dev_info_t.modbus_tcp_port;

	 mqtt2_cfg.credentials.authentication.password =SetData.dev_info_t.Net_Server_secret;

    g_self_data.mod_reg12000_IOT_set.LCD_Mode.all = SetData.dev_info_t.LCD_Mode.all;
    g_self_data.mod_reg12000_IOT_set.Time_Span.all = SetData.dev_info_t.Time_Span.all;
    g_self_data.mod_reg11000_IOT_info.saveMoneyNums = SetData.dev_info_t.saveMoneyNums; // 省钱金额
    g_self_data.mod_reg11000_IOT_info.powerOff_Nums = SetData.dev_info_t.powerOff_Nums;
    g_self_data.mod_reg02000_Inv_base_set.ctrl_lcd_active_time = SetData.dev_info_t.ctrl_lcd_active_time;

	g_self_data.mod_reg00000.modbus_ver = MODBUS_VERSION;// SetData.dev_info_t.protocol_ver
	g_self_data.mod_reg00000.modbus_ver_iot = 4;
	//g_self_data.mod_reg00000.support_mode.bit.support_ble_pwd = 1; //蓝牙密码设置区
	//g_self_data.mod_reg00000.support_mode.bit.visitor_mode=2;
	g_self_data.mod_reg11000_IOT_info.software_ver = SetData.dev_info_t.software_ver;
	#ifdef CONFIG_MORE_WIFI_STA_LINK_ENABLE
	g_self_data.mod_reg11000_IOT_info.wifi_mult_sta_flag.bit.ext_wifi_enable_num = 2; // 额外支持STA2和STA3
 	#endif
     // 电表配置复制到modbus, [METER_MAX_NUM]为汇总
     for (int i = 0; i < METER_MAX_NUM; i++)
     {
        // Meter[i].mod_reg01900_meter.meter_type = SetData.dev_info_t.meter_cfg[i].meter_type;
        Meter[i].mod_reg01900_meter.dev_type = SetData.dev_info_t.meter_cfg[i].dev_type;
        Meter[i].mod_reg01900_meter.mfg_id = SetData.dev_info_t.meter_cfg[i].mfg_id;
        Meter[i].mod_reg01900_meter.func = SetData.dev_info_t.meter_cfg[i].func;
        memcpy(Meter[i].mod_reg01900_meter.dev_id, SetData.dev_info_t.meter_cfg[i].dev_id,
                sizeof(Meter[i].mod_reg01900_meter.dev_id));

        ESP_LOGI(TAG, "Meter[%s].dev type:%d, mfg id:%d, func:%u, last online time:%lu",
            Meter[i].mod_reg01900_meter.dev_id,
            Meter[i].mod_reg01900_meter.dev_type, Meter[i].mod_reg01900_meter.mfg_id,
            Meter[i].mod_reg01900_meter.func,
            Meter[i].mod_reg01700_meter.last_online_time);
     }
	 
	 g_self_data.mod_reg11000_IOT_info.Bind_SN =((uint16_t)SetData.dev_info_t.Sn_bind[1]<<8)|SetData.dev_info_t.Sn_bind[0];
	 ESP_LOGI(TAG, "11bk_iot_dev_node. .Net_Server_address	  =%s",g_self_data.mod_reg22000_net_server_2rd.Net_Server_address);
	 ESP_LOGI(TAG, "11bk_iot_dev_node. .Net_Server_secret	 =%s",g_self_data.mod_reg22000_net_server_2rd.Net_Server_secret);
 }
 /*------------------------------------------------------------------------------
  Function: GetInitDataFromLittlefs
  -----------------------------------------------------------------------------*/
 /**
   * @brief 	 首次上电，从flash读取能量信息（该函数须在eeprom初始数据获取后才可调用）
   * @param[in]  void  
   * @param[out] None
   * @return	 void
   */
int GetInitDataFromLittlefs(void)
{
	char path[50] = {0};
	char fname[30] = {0};
	int fd;

	uint32_t iot_version = IOT_VERSION_AP200; // 默认没有标定时

	get_parameter_from_flash();
	get_parameter_from_flash_2();
	get_relay_parameter_from_flash();       //RELAY
	SetData.dev_info_t.on_off.bit.ble_enable = 1;
    // 线上升级，若未设置过广播key，则使用默认值
    if (array_is_empty(SetData.dev_info_t.bles_adv_key, sizeof(SetData.dev_info_t.bles_adv_key)))
    {
        ESP_LOGW(TAG, "ble adv key is empty, use default key");
        memcpy(SetData.dev_info_t.bles_adv_key, BLE_ADV_KEY_DEFAULT, sizeof(SetData.dev_info_t.bles_adv_key));
    }
	if (SetData.dev_info_t.ble_protocol.adv_en == 0)
	{
		ESP_LOGW(TAG, "ble adv en is empty, use default set");
		SetData.dev_info_t.ble_protocol.adv_en = 1;
		SetData.dev_info_t.ble_protocol.lcd_adv_en = 1;
    }
    /* 线上升级，若未启动AP，则打开AP */
    if (SetData.dev_info_t.on_off.bit.wifi_ap_enable == 0)
    {
        ESP_LOGW(TAG, "Wi-Fi AP is closed, open Wi-Fi AP");
        SetData.dev_info_t.on_off.bit.wifi_ap_enable = 1; //默认开启AP

        char wifi_ap_ssid[32] = {0};
        snprintf(wifi_ap_ssid, sizeof(wifi_ap_ssid), "%s%llu", iot_factory.iot_type, iot_factory.iot_sn);
        memcpy(SetData.dev_info_t.wifi_ap_ssid, wifi_ap_ssid, sizeof(wifi_ap_ssid));

        //注意: AP密码必须超过8个字符
        char wifi_ap_password[64] = {0};
        uint8_t md5_value[16] = {0};
        snprintf(wifi_ap_password, sizeof(wifi_ap_password), "%s%llu", iot_factory.iot_type, iot_factory.iot_sn);
        // 32位小写MD5
        calculate_md5((uint8_t *)wifi_ap_password, strlen(wifi_ap_password), md5_value);
        snprintf(wifi_ap_password, sizeof(wifi_ap_password), "%02x%02x%02x%02x%02x%02x%02x%02x",
            md5_value[0], md5_value[2], md5_value[8], md5_value[3], md5_value[4], md5_value[6], md5_value[12], md5_value[10]);
        memset(SetData.dev_info_t.wifi_ap_password, 0x00, sizeof(SetData.dev_info_t.wifi_ap_password));
        memcpy(SetData.dev_info_t.wifi_ap_password, wifi_ap_password, sizeof(wifi_ap_password));

        SetData.dev_info_t.wifi_ap_auth = WIFI_AUTH_WPA2_PSK;
    }

	  InitIotKwhFile(KWH_RECORD_FILE_MARK_GRID,&reals.energy_GridChgin_Total);
	  InitIotKwhFile(KWH_RECORD_FILE_MARK_FD,&reals.energy_GridFeedback_Total);

	  InitIotKwhFile(KWH_RECORD_FILE_MARK_AC_LOAD,&reals.energy_ACLoad_Total);
	  InitIotKwhFile(KWH_RECORD_FILE_MARK_DC_LOAD,&reals.energy_DCLoad_Total);

	  InitIotKwhFile(KWH_RECORD_FILE_MARK_ACPV,&reals.energy_pv_ac);
	  InitIotKwhFile(KWH_RECORD_FILE_MARK_DCPV,&reals.energy_Pv_dc);

	  InitIotKwhFile(KWH_RECORD_FILE_MARK_BATDICH,&reals.energy_PackDsg_Total);
	  InitIotKwhFile(KWH_RECORD_FILE_MARK_PV_TO_LOAD,&reals.energy_PVToload_Total);
		  



//windy debug force
//	SetData.dev_info_t.historyAddrIndex =0;
//	SetData.dev_info_t.historyRecSaveCount =0;
//
//	SetData.dev_info_t.Event_SaveCount =0;
//	SetData.dev_info_t.Event_AddrIndex =0;


	reals.historyRecSaveCount =SetData.dev_info_t.historyRecSaveCount;
	reals.historyAddrIndex =SetData.dev_info_t.historyAddrIndex;				
	reals.Event_SaveCount =SetData.dev_info_t.Event_SaveCount;
	reals.Event_AddrIndex =SetData.dev_info_t.Event_AddrIndex;
	reals.Addr_can_self =SetData.dev_info_t.Addr_can_self;	
	reals.Addr_can_Buff =SetData.dev_info_t.Addr_can_self;
//	file_list_clear();//debug

	if (strcmp(SetData.dev_info_t.INV_dev_type, IOT_TYPE_AP300) == 0)
	{
		iot_version = IOT_VERSION_AP300;
	}
	else
	{
		iot_version = IOT_VERSION_AP200;
	}

    /*软件版本定义*/
    SetData.dev_info_t.software_ver = iot_version;

	//标定信息（标定类型，标定SN，标定安全码）
//	g_device_data.iot_dev_node.iot_about.software_ver =SetData.dev_info_t.software_ver;
	g_self_data.mod_reg11000_IOT_info.software_ver = iot_version;
	memcpy(&g_self_data.mod_reg00000.support_mode,&SetData.dev_info_t.support_mode,sizeof(SetData.dev_info_t.support_mode));
	if(!g_self_data.mod_reg00000.support_mode.bit.visitor_mode)
	{
		g_self_data.mod_reg00000.support_mode.bit.visitor_mode=2;
		SetData.dev_info_t.support_mode.bit.visitor_mode=2;
	}
	g_self_data.mod_reg00000.support_mode.bit.support_ble_pwd=1;
	// g_self_data.mod_reg00000.support_mode.bit.visitor_mode=2;
//	  memcpy(MicroInv[0].mod_reg14000_HMI_info.hmi_type, iot_factory.iot_type, sizeof(iot_factory.iot_type));
//	  MicroInv[0].mod_reg14000_HMI_info.hmi_sn = iot_factory.iot_sn;
//	  MicroInv[0].mod_reg14000_HMI_info.mcu_ver = HMI1_VERSION;
//	  MicroInv[0].mod_reg14000_HMI_info.flash_ver = HMI2_VERSION;

	//strcpy(SetData.dev_info_t.could_dns, "dev.iot.poweroak.ltd:18760");//windy debug 测试服务器
	// strcpy(SetData.dev_info_t.could_dns, DEV_SERVER_URL);//windy debug 测试服务器

	ESP_LOGE(TAG, "windy 1 ,SetData.dev_info_t.could_dns = %s" ,SetData.dev_info_t.could_dns);

	ESP_LOGI(TAG,"g_self_data.mod_reg00000.support_mode:%u",g_self_data.mod_reg00000.support_mode.all);

	ESP_LOGI(TAG, "software version: %lu, sub_verion: %d", iot_version, SUB_VERSION);

	ESP_LOGI(TAG, "server address: %s", SetData.dev_info_t.could_dns);
	
	uint8_t *pw = (uint8_t *)SetData.dev_info_t.app_password;//g_self_data.mod_reg00000.app_password;
	ESP_LOGI(TAG, "app password: %02x %02x %02x %02x %02x %02x", pw[0], pw[1], pw[2], pw[3], pw[4], pw[5]);
	ESP_LOGI(TAG, "thunder storm enable: %d", SetData.dev_info_t.thunder_ctrl.thunder_enable);

	ESP_LOGI(TAG, "wifi name SetData: %s, password: %s, auth mode: %d", SetData.dev_info_t.wifi_sta_ssid, SetData.dev_info_t.wifi_sta_password, SetData.dev_info_t.wifi_sta_auth);

	ESP_LOGI(TAG, "wifi name:g_self_data  %s, password: %s, auth mode: %d", SetData.dev_info_t.wifi_sta_ssid, SetData.dev_info_t.wifi_sta_password, SetData.dev_info_t.wifi_sta_auth);
    ESP_LOGI(TAG, "wifi ap enable:%d ssid:%s, password: %s, auth mode: %d", SetData.dev_info_t.on_off.bit.wifi_ap_enable, SetData.dev_info_t.wifi_ap_ssid, SetData.dev_info_t.wifi_ap_password, SetData.dev_info_t.wifi_ap_auth);

	memcpy(login2_info.raw_url, SetData.dev_info_t.Net_Server_address, sizeof(SetData.dev_info_t.Net_Server_address));
	mqtt2_cfg.credentials.authentication.password =SetData.dev_info_t.Net_Server_secret;

    for (int i = 0; i < METER_MAX_NUM; i++)
    {
        ESP_LOGI(TAG, "SetData meter hostname[%s].dev type:%d, mfg id:%d, func:%u", SetData.dev_info_t.meter_cfg[i].dev_id,
            SetData.dev_info_t.meter_cfg[i].dev_type, SetData.dev_info_t.meter_cfg[i].mfg_id,
            SetData.dev_info_t.meter_cfg[i].func);
    }

    /*更新本地参数至modbus寄存器*/
	  return 0;
  
 
 }

/*------------------------------------------------------------------------------
 Function: GetYearXIotDataFromExtFlash
 -----------------------------------------------------------------------------*/
/**
  * @brief      获取modbus3500段汇总数据
  * @param[in]  uint16_t energy_type  
  * @param[out] None
  * @return     int
  */
int GetYearXIotDataFromExtFlash(uint16_t energy_type)
{
     uint32_t file_bias = 0;
     char path[30] = {0};
     int fd;
     uint8_t year = SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_year;
     uint8_t year_cnt = 0;
     uint8_t year_min = (MAX_KWH_STORE_YEAR < (SetData.dev_info_t.iot_kwh_info.kwh_years_num + 1)) ? MAX_KWH_STORE_YEAR : (SetData.dev_info_t.iot_kwh_info.kwh_years_num + 1);
     
     if (energy_type == PV_ENERGY_TYPE)
     {
//       sprintf(path, "%s/%s%02u", RECORD_ROOT_PATH, KWH_RECORD_FILE_MARK_PV, year); 
//       sprintf(path, "%s/%s_year", RECORD_ROOT_PATH, KWH_RECORD_FILE_MARK_PV);
         sprintf(path, "%s/%s%d_%02d", RECORD_ROOT_PATH, KWH_RECORD_FILE_MARK_DCPV, 0, 0);
     }
     else if(energy_type == GRID_ENERGY_INPUT_TYPE)
     {
         sprintf(path, "%s/%s%d_%02d", RECORD_ROOT_PATH, KWH_RECORD_FILE_MARK_GRID, 0, 0);
     }
     else if(energy_type == GRID_ENERGY_OUTPUT_TYPE)
     {
         sprintf(path, "%s/%s%d_%02d", RECORD_ROOT_PATH, KWH_RECORD_FILE_MARK_FD, 0, 0);
     }
     else if(energy_type == AC_LOAD_ENERGY_TYPE)
     {
         sprintf(path, "%s/%s%d_%02d", RECORD_ROOT_PATH, KWH_RECORD_FILE_MARK_AC_LOAD, 0, 0);
     }
	 
      else if(energy_type == DC_LOAD_ENERGY_TYPE)
      {
          sprintf(path, "%s%s%d_%02d", RECORD_ROOT_PATH, KWH_RECORD_FILE_MARK_DC_LOAD, 0, 0);
      }
      else if(energy_type == AC_PV_ENERGY_TYPE)
      {
          sprintf(path, "%s%s%d_%02d", RECORD_ROOT_PATH, KWH_RECORD_FILE_MARK_ACPV, 0, 0);
      }
      else if(energy_type == DC_PV_ENERGY_TYPE)
      {
          sprintf(path, "%s%s%d_%02d", RECORD_ROOT_PATH, KWH_RECORD_FILE_MARK_DCPV, 0, 0);
      }
      else if(energy_type == BAT_TOTAL_CHARGE_ENERGY_TYPE)
      {
          sprintf(path, "%s%s%d_%02d", RECORD_ROOT_PATH, KWH_RECORD_FILE_MARK_BATDICH, 0, 0);
      }
      else if(energy_type == BAT_TOTAL_DISCHARGE_ENERGY_TYPE)
      {
          sprintf(path, "%s%s%d_%02d", RECORD_ROOT_PATH, KWH_RECORD_FILE_MARK_BATDICH, 0, 0);
      }
      else if(energy_type == PV_TO_ACLOAD_ENERGY_TYPE)
      {
          sprintf(path, "%s%s%d_%02d", RECORD_ROOT_PATH, KWH_RECORD_FILE_MARK_PV_TO_LOAD, 0, 0);
      }
     else//无效类型
     {
         return -1;
     }
 
     /* 只读格式打开文件 */
     fd = open(path, O_RDONLY);
     if(fd < 0)
     {
         ESP_LOGE(TAG, "[GetYearXDataFromLittlefs] open error(%s)",path);
         return 0;
     }
         
     while(year_min - year_cnt)
     {
         /* 移动文件指针到设置的数据位置 */
         file_bias = (SetData.dev_info_t.iot_kwh_info.kwh_years_num - year_cnt) * sizeof(year_energy_t);
         lseek(fd, (file_bias + 1), SEEK_SET);
        //  {
            //  //ESP_LOGE(TAG, "[GetYearXDataFromLittlefs] start_pos set error(%s)(%d)(%d),%d", path, year, year_cnt,lseek(fd, (file_bias + 1), SEEK_SET));
            //  goto next;
        //  }
     
         /* 读数据到缓存 */
//         if(read(fd, Data_Record[0].year_data+year_cnt*sizeof(_year_energy1), sizeof(_year_energy1)) != sizeof(_year_energy1)) 
		if(read(fd, &Inv[0].mod_reg03500_Inv_yearX_statistic.year_data[year_cnt], sizeof(_year_energy1)) != sizeof(_year_energy1))  	
         {
             ESP_LOGE(TAG, "[GetYearXDataFromLittlefs] read error(%s)(%d)(%d)", path, year, year_cnt);
             goto next;
         }
        // //ESP_LOGE(TAG, "[GetYearXDataFromLittlefs] yearX_energy init ok(%s)(%d)(%d)", path, year, year_cnt);
        //  //ESP_LOGI(TAG, "[GetYearXDataFromLittlefs] yearX_energy init ok(%s)(%d)(%d),file_bias:%d,%04x", path, year, year_cnt,file_bias,Data_Record[0].year_data+year_cnt);
        //  //ESP_LOG_BUFFER_HEX(TAG, Data_Record[0].year_data+year_cnt, sizeof(_year_energy1));
        next:
         year_cnt++;
         year--;
     }
     
     close(fd);
     
     return 0;

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
int GetHistoryLogFromLittlefs(uint16_t history_page)
{
     char path[30] = {0};
     int fd;
     uint16_t page_size = 5 * sizeof(reals.log_fault_info);//每页最大字节数
     uint16_t max_size = 100 * sizeof(reals.log_fault_info);//最多存储100条
     uint8_t pages;//实际总页数
     uint16_t file_bias;//文件偏移
     uint16_t read_size;//读取长度

//	 uint16_t file_bias_addr;//换算后物理偏移
	 uint32_t file_pos=0;
	 uint32_t u32Templen=0;
	 uint32_t u32Templen2=0;
	 
	 uint32_t u32Tempdata=0;
	 
	 memset(&g_self_data.mod_reg03000_Inv_history, 0, sizeof(MOD_STRUCT_reg03000));
	 
     g_self_data.mod_reg03000_Inv_history.current_page_seq = history_page;

//     INV_FILE_PATH(path,1,1);
    // sprintf(path, "%s/%s%s%d_%d", RECORD_ROOT_PATH, LOG_RECORD_FOLDER_LEVEL1, INV_RECORD_FILE_MARK, 15, 0);
 	sprintf(path, "%s/%s%s%d_%02d", RECORD_ROOT_PATH, LOG_RECORD_FOLDER_LEVEL1, INV_RECORD_FILE_MARK, 15, 0);
     /* 只读格式打开文件 */
     fd = open(path, O_RDONLY);
     if(fd < 0)
     {
         ESP_LOGE(TAG, "[GetHistoryLogFromLittlefs] open error(%s), page(%d)",path, history_page);
         memset((uint8_t *)&g_self_data.mod_reg03000_Inv_history, 0, sizeof(g_self_data.mod_reg03000_Inv_history));
         return 0;
     }

     // 移动到文件末尾
     off_t fileSize = lseek(fd, 0, SEEK_END); // 获取文件大小
	 ESP_LOGE(TAG, "fileSize is : %lu", fileSize);

     if (fileSize > max_size)
     {
         fileSize = max_size;//最多存储100条
     }
     
     pages = (uint8_t)(fileSize/page_size)+1; // 计算文件页大小
	 file_bias = (history_page + 0)*page_size;
	 ESP_LOGE(TAG, "pages is : %d", pages);



//////////

	 
	//  u32Templen =reals.historyRecSaveCount*HISTORY_ONE_BYTE_COUNT;
	 u32Templen =fileSize/10*HISTORY_ONE_BYTE_COUNT;
	 ESP_LOGE(TAG, "u32Templen is : %lu", u32Templen);
	  if(u32Templen >= file_bias)
	  {
		   u32Tempdata =(u32Templen - file_bias);

		  if(0 == u32Tempdata )
		  {
			  read_size =0;//tbd  break
		  }
		  if(u32Tempdata <= page_size)
		  {
			  read_size =u32Tempdata;
		  }
		  else
		  {
			  read_size =page_size;
		  }
	 
		   if (reals.historyAddrIndex < reals.historyRecSaveCount) //	full（已经覆盖到下一圈）
		   {
			   if((reals.historyAddrIndex*HISTORY_ONE_BYTE_COUNT+ file_bias + read_size) < HISTORY_MAX_COUNT*HISTORY_ONE_BYTE_COUNT)//结尾<max
			   {
				   file_pos =  reals.historyAddrIndex*HISTORY_ONE_BYTE_COUNT+ file_bias ;// 
				   u32Templen  =read_size;

					if(lseek(fd, file_pos, SEEK_SET) != file_pos) 
					{
						goto end;
					}	 
					if(read(fd, &g_self_data.mod_reg03000_Inv_history.log_data, u32Templen) != u32Templen) 
					{
						ESP_LOGE(TAG, "[GetHistoryLogFromLittlefs] read error(%s)",path);
						goto end;
					}				   
			   }
			   else//结尾>max，分段读取
			   {
	 
				   if(reals.historyAddrIndex*HISTORY_ONE_BYTE_COUNT+ file_bias < (HISTORY_MAX_COUNT*HISTORY_ONE_BYTE_COUNT))//start < max
				   {
					   file_pos =  reals.historyAddrIndex*HISTORY_ONE_BYTE_COUNT+ file_bias ;// 
					   u32Templen  =HISTORY_MAX_COUNT*HISTORY_ONE_BYTE_COUNT -(reals.historyAddrIndex*HISTORY_ONE_BYTE_COUNT+ file_bias);
					   u32Templen2= u32Templen;
	 
					   if(lseek(fd, file_pos, SEEK_SET) != file_pos) 
					   {
						   goto end;
					   }	
					   if(read(fd, &g_self_data.mod_reg03000_Inv_history.log_data, u32Templen) != u32Templen) 
					   {
						   ESP_LOGE(TAG, "[GetHistoryLogFromLittlefs] read error(%s)",path);
						   goto end;
					   }				  
					   ////////////////
					   if(u32Templen2 < page_size)//buf len 限制
					   {
						   file_pos = 0 ;// 
						   u32Templen =reals.historyAddrIndex*HISTORY_ONE_BYTE_COUNT+ file_bias+read_size -HISTORY_MAX_COUNT*HISTORY_ONE_BYTE_COUNT; 				   

						   if(lseek(fd, file_pos, SEEK_SET) != file_pos) 
						   {
							   goto end;
						   }	
						   if(read(fd, &g_self_data.mod_reg03000_Inv_history.log_data[u32Templen2/page_size], u32Templen) != u32Templen) 
						   {
							   ESP_LOGE(TAG, "[GetHistoryLogFromLittlefs] read error(%s)",path);
							   goto end;
						   }				  
						   
					   }
				   }
				   else//start > max
				   {
					   file_pos = reals.historyAddrIndex*HISTORY_ONE_BYTE_COUNT+ file_bias - HISTORY_MAX_COUNT*HISTORY_ONE_BYTE_COUNT;
					   u32Templen  =read_size;	   
					   if(lseek(fd, file_pos, SEEK_SET) != file_pos) 
					   {
						   goto end;
					   }	
					   if(read(fd, &g_self_data.mod_reg03000_Inv_history.log_data, u32Templen) != u32Templen) 
					   {
						   ESP_LOGE(TAG, "[GetHistoryLogFromLittlefs] read error(%s)",path);
						   goto end;
					   }				  
	 
				   }
			   
			   }   
				  
		   }
		   else if( reals.historyAddrIndex == reals.historyRecSaveCount)	  // == not full(还没有覆盖一圈)
		   {
			  file_pos = file_bias ;// 
			  u32Templen  =read_size;	  
			  if(lseek(fd, file_pos, SEEK_SET) != file_pos) 
			  {
				  goto end;
			  }    
			  if(read(fd, &g_self_data.mod_reg03000_Inv_history.log_data, u32Templen) != u32Templen) 
			  {
				  ESP_LOGE(TAG, "[GetHistoryLogFromLittlefs] read error(%s)",path);
				  goto end;
			  } 				 
		   }	  
	  }
	  else//非法
	  {
		  //越界访问，访问页不存在
		  memset((uint8_t *)&g_self_data.mod_reg03000_Inv_history, 0, sizeof(g_self_data.mod_reg03000_Inv_history));
		  goto end;
	  }



     end:


     close(fd);
     
     return pages;

}


/*------------------------------------------------------------------------------
 Function: GetHistoryLogFromLittlefsReversal
 -----------------------------------------------------------------------------*/
/**
  * @brief      modbus3000历史记录读取 倒序读取
  * @param[in]  uint16_t history_page  
  * @param[out] None
  * @return     int
  */
int GetHistoryLogFromLittlefsReversal(uint16_t history_page)
{
	char path[30] = {0};
	int fd;
	uint16_t page_size = 5 * sizeof(reals.log_fault_info);//每页最大字节数
	uint16_t max_size = 100 * sizeof(reals.log_fault_info);//最多存储100条
	uint8_t pages;//实际总页数
	uint16_t file_bias=0;//文件偏移1
	uint16_t file_bias2 =0;//文件偏移2
	uint16_t read_size=0;//读取长度
	uint16_t read_size2=0;//读取长度

//	 uint16_t file_bias_addr;//换算后物理偏移
	uint32_t file_pos=0;
	uint32_t u32Templen=0;
	
	memset(&g_self_data.mod_reg03000_Inv_history, 0, sizeof(MOD_STRUCT_reg03000));
	
	g_self_data.mod_reg03000_Inv_history.current_page_seq = history_page;

//     INV_FILE_PATH(path,1,1);
	sprintf(path, "%s/%s%s%d_%02d", RECORD_ROOT_PATH, LOG_RECORD_FOLDER_LEVEL1, INV_RECORD_FILE_MARK, 15, 0);

	/* 只读格式打开文件 */
	fd = open(path, O_RDONLY);
	if(fd < 0)
	{
		ESP_LOGE(TAG, "[GetHistoryLogFromLittlefs] open error(%s), page(%d)",path, history_page);
		memset((uint8_t *)&g_self_data.mod_reg03000_Inv_history, 0, sizeof(g_self_data.mod_reg03000_Inv_history));
		return 0;
	}

	// 移动到文件末尾
	off_t fileSize = lseek(fd, 0, SEEK_END); // 获取文件大小
	ESP_LOGE(TAG, "fileSize is : %lu max_size:%d page_size:%d historyAddrIndex:%d historyRecSaveCount:%d", fileSize,max_size,page_size,reals.historyAddrIndex,reals.historyRecSaveCount);

	if (fileSize > max_size)
	{
		fileSize = max_size;//最多存储100条
	}
	
	if(0==(fileSize%page_size))// 计算文件页大小
	{
		pages = (uint8_t)(fileSize/page_size); 
	}
	else
	{
		pages = (uint8_t)(fileSize/page_size)+1; 
	}

	if(history_page>pages)
	{
		ESP_LOGE(TAG, "Read Pages:%d Over TotalPage:%d ", history_page,pages);
		memset((uint8_t *)&g_self_data.mod_reg03000_Inv_history, 0, sizeof(g_self_data.mod_reg03000_Inv_history));
		return 0;
	}

	ESP_LOGE(TAG, "pages is : %d", pages);
	//读取告警历史从文件尾开始读
	if (reals.historyAddrIndex == reals.historyRecSaveCount)
	{
		if(fileSize>(page_size*(history_page+1)))
		{
			read_size=page_size;
			file_bias = fileSize-(page_size*(history_page+1));
		}
		else 
		{
			read_size=fileSize-(page_size*history_page);//读取到最后一页，该页警报记录可能少于5条
			file_bias=0;
		}

		ESP_LOGE(TAG, "A file_bias is : %d", file_bias);
		goto read1;

	}else if(reals.historyAddrIndex < reals.historyRecSaveCount)
	{
		if((reals.historyAddrIndex*HISTORY_ONE_BYTE_COUNT)>=page_size*(history_page+1))
		{
			file_bias=(reals.historyAddrIndex*HISTORY_ONE_BYTE_COUNT)-(page_size*(history_page+1));
			read_size=page_size;
			ESP_LOGE(TAG, "B file_bias is : %d  read_size:%d", file_bias,read_size);
			goto read1;
		}
		else
		{
			file_bias=0;
			file_bias2=fileSize-(page_size*(history_page+1)-(reals.historyAddrIndex*HISTORY_ONE_BYTE_COUNT));
			if((page_size*history_page)>(reals.historyAddrIndex*HISTORY_ONE_BYTE_COUNT))
			{
				read_size=0;
			}
			else
			{
				read_size=(reals.historyAddrIndex*HISTORY_ONE_BYTE_COUNT)-(page_size*history_page);
			}
			read_size2=page_size-read_size;
			//read_size2=page_size*(history_page+1)-(reals.historyAddrIndex*HISTORY_ONE_BYTE_COUNT);
			ESP_LOGE(TAG, "C file_bias is : %d file_bias2 is:%d read_size:%d read_size2:%d", file_bias,file_bias2,read_size,read_size2);
			goto read2;

		}
	}
	else
	{
		//越界访问，访问页不存在
		memset((uint8_t *)&g_self_data.mod_reg03000_Inv_history, 0, sizeof(g_self_data.mod_reg03000_Inv_history));
		goto end;
	}

	
	
	read1:
	{
		ESP_LOGE(TAG, "goto read1");
		file_pos = file_bias ;// 
		u32Templen =fileSize/10*HISTORY_ONE_BYTE_COUNT;

		ESP_LOGE(TAG, "u32Templen is : %lu", u32Templen);
		if(u32Templen >= file_bias)
		{
			ESP_LOGW(TAG, "1 file_pos:%d,read_size:%d",(unsigned int)file_pos,(unsigned int)read_size);
			if(lseek(fd, file_pos, SEEK_SET) != file_pos) 
			{
				ESP_LOGW(TAG, "1 lseek");  
				goto end;
			}	 
			if(read(fd, &g_self_data.mod_reg03000_Inv_history.log_data, read_size) != read_size) 
			{
				ESP_LOGE(TAG, "[GetHistoryLogFromLittlefs] read error(%s)",path);
				goto end;
			}
			goto end;
	  	}
		else//非法
	  	{
			//越界访问，访问页不存在
			memset((uint8_t *)&g_self_data.mod_reg03000_Inv_history, 0, sizeof(g_self_data.mod_reg03000_Inv_history));
			goto end;
	  	}
	}

	read2: //分段读取 （0~reals.historyAddrIndex*10）+（fileSize-（readsize-reals.historyAddrIndex*10）,fileSize）
	{
		ESP_LOGE(TAG, "goto read2");
		u32Templen=fileSize/10*HISTORY_ONE_BYTE_COUNT;
		file_pos = file_bias ;//
		if((u32Templen >= file_bias)&&(u32Templen>=file_bias2))
		{
			ESP_LOGI(TAG, "2 file_pos:%d,read_size:%d,read_size2:%d",(unsigned int)file_pos,(unsigned int)read_size,(unsigned int)read_size2);
			//读取（0~reals.historyAddrIndex*10）
			if(read_size)
			{
				if(lseek(fd, file_pos, SEEK_SET) != file_pos) 
				{
					ESP_LOGW(TAG, "2 lseek Err");  
					goto end;
				}
				if(read(fd, &g_self_data.mod_reg03000_Inv_history.log_data[read_size2/HISTORY_ONE_BYTE_COUNT], read_size) != read_size) 
				{
					ESP_LOGE(TAG, "2[GetHistoryLogFromLittlefs] read error(%s)",path);
					goto end;
				}
			}	

			//读取（fileSize-（readsize-reals.historyAddrIndex*10）,fileSize）
			file_pos = file_bias2 ;//
			if(read_size2)
			{
				if(lseek(fd, file_pos, SEEK_SET) != file_pos) 
				{
					ESP_LOGW(TAG, "3 lseek Err");  
					goto end;
				}
				if(read(fd, &g_self_data.mod_reg03000_Inv_history.log_data, read_size2) != read_size2) 
				{
					ESP_LOGE(TAG, "3[GetHistoryLogFromLittlefs] read error(%s)",path);
					goto end;
				}	
			}

			goto end;

		}
		else//非法
	  	{
			//越界访问，访问页不存在
			memset((uint8_t *)&g_self_data.mod_reg03000_Inv_history, 0, sizeof(g_self_data.mod_reg03000_Inv_history));
			goto end;
	  	}
	}

//////////

    end:


     close(fd);
     
     return pages;

}


/*------------------------------------------------------------------------------
 Function: check_with_conditional_resize
 -----------------------------------------------------------------------------*/
/**
  * @brief      检查当前文件长度，使其不超过2k
  * @param[in]  const char* filename  
  * @param[out] None
  * @return     void
  */
void check_with_conditional_resize(const char* filename) 
{
    //最多存储100条
    uint16_t max_size = 100 * sizeof(reals.log_fault_info);
    uint8_t *buffer = NULL;
    
    // 使用open以读写模式打开文件，如果文件不存在则退出
    int file = open(filename, O_RDWR);
    if (file < 0) {
        return;
    }

    // 移动到文件末尾
    off_t fileSize = lseek(file, 0, SEEK_END); // 获取当前文件大小

    if (fileSize >= (2*max_size)) {
        // 如果文件大于等于2KB，读取最后1KB数据
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


/*

历史记录的计数标记同步；
平衡 历史记录不断增加和读取同时发生的读取内容错乱
*/
void History_Cnt_Read_Flag_Update(void ) //100ms
{
	static uint32_t sdely1=0; 
	static uint32_t sdely2=0; 

	if(SetData.dev_info_t.historyRecSaveCount >= reals.historyRecSaveCount)
	{
		if(++sdely1 >= 100)//记录增加，延时更新，防止快速更新导致读取历史记录无法停止
		{
			sdely1 =0;
			reals.historyRecSaveCount =SetData.dev_info_t.historyRecSaveCount;
			reals.historyAddrIndex =SetData.dev_info_t.historyAddrIndex;				
		}
	}
	else//减小、清零，快速更新
	{
		sdely1 =0;
	
		reals.historyRecSaveCount =SetData.dev_info_t.historyRecSaveCount;
		reals.historyAddrIndex =SetData.dev_info_t.historyAddrIndex;	
	}
	
///////////

	if(SetData.dev_info_t.Event_SaveCount >= reals.Event_SaveCount)
	{
		if(++sdely2 >= 100)//记录增加，延时更新，防止快速更新导致读取历史记录无法停止
		{
			sdely2 =0;
			reals.Event_SaveCount =SetData.dev_info_t.Event_SaveCount;
			reals.Event_AddrIndex =SetData.dev_info_t.Event_AddrIndex;				
		}
	}
	else//减小、清零，快速更新
	{
		sdely2 =0;
	
		reals.Event_SaveCount =SetData.dev_info_t.Event_SaveCount;
		reals.Event_AddrIndex =SetData.dev_info_t.Event_AddrIndex;	
	}


}

/*
从已保存到缓存的历史记录队列写入到flash

return:
0-ok
other-fail
*/
uint8_t Inv_Fault_Log_Pop(void)// 
{
	LOG_FAULT_STRUCT_queue_struct queue_msg =  {NULL};
	char fname[30] = {0};
	uint32_t file_bias =0;
	uint8_t rtn=0xFF;
//	ESP_LOGI(TAG, "Logger_PopAA 	");

	if(xQueue_Log_record && xQueueReceive(xQueue_Log_record, &queue_msg, 0) == pdTRUE)//pdMS_TO_TICKS(task_delay)
	{

	ESP_LOGI(TAG, "Inv_Fault_Log_Pop , node id : %d	", queue_msg.INV_NUM);

		INV_FILE_PATH(fname,queue_msg.INV_NUM,0);
		
		file_bias = (uint16_t)SetData.dev_info_t.historyAddrIndex;// *sizeof(LOG_FAULT_STRUCT);

		if(0 == SetData.dev_info_t.historyAddrIndex)
		{
//			remove(fname);
//			file_list_clear();//debug
		}
//		if (0 == historic_data_write(fname, (uint8_t *)&reals.log_fault_info, file_bias, sizeof(LOG_FAULT_STRUCT))) 
		if (0 == historic_data_write(fname, (uint8_t *)queue_msg.pdata, file_bias, sizeof(LOG_FAULT_STRUCT))) 
		{
			dump_buf("historic_data_write：：", (uint8_t *)queue_msg.pdata, sizeof(LOG_FAULT_STRUCT));

			 ESP_LOGI(TAG, "File written ok (%s)",fname);
			 rtn =0;

			 if(0 == rtn)	 
			 {
				 SetData.dev_info_t.historyAddrIndex++;
				 if(SetData.dev_info_t.historyAddrIndex >= HISTORY_MAX_COUNT)
				 {
					 SetData.dev_info_t.historyAddrIndex=0;
				 }
				 else
				 {
				 }
			 
				 SetData.dev_info_t.historyRecSaveCount++;
				 if(SetData.dev_info_t.historyRecSaveCount > HISTORY_MAX_COUNT )	 //100		 
				 {
					 SetData.dev_info_t.historyRecSaveCount = HISTORY_MAX_COUNT;
				 }
							 
//				  if((SetData.dev_info_t.historyRecSaveCount < SetData.dev_info_t.historyAddrIndex)
//					  ||((SetData.dev_info_t.historyRecSaveCount > SetData.dev_info_t.historyAddrIndex)&&(HISTORY_MAX_COUNT != SetData.dev_info_t.historyRecSaveCount)))//纠错处理
//				 {
//					 SetData.dev_info_t.historyRecSaveCount=0;
//					 SetData.dev_info_t.historyAddrIndex=0;
//				 }	 
			 
				  reals.SetDataWrFlag.sBit.SetDataUpdate_historycnt =1;				 
			 }
			 
		}
		else
		{
			ESP_LOGE(TAG, "File written fail (%s)",fname );
		}
		
		INV_FILE_PATH(fname,0x0f,0);//存储汇总文件
		if(0 == historic_data_write(fname, (uint8_t *)queue_msg.pdata, file_bias, sizeof(LOG_FAULT_STRUCT)))
		{
			 ESP_LOGI(TAG, "File written ok (%s)",fname);
		}
		else
		{
			ESP_LOGE(TAG, "File written fail (%s)",fname );
		}
	
		if (queue_msg.pdata) 
		{
			free(queue_msg.pdata); /* 处理完成释放CAN数据帧 */
			queue_msg.pdata = NULL; 
	
		}
	
	}
return rtn;
}
/*
实时告警记录存储到队列缓存
*/
void Inv_Fault_Log_Push(uint8_t node_id)//100ms cycle
{
	LOG_FAULT_STRUCT_queue_struct queue_msg =  {NULL};

	queue_msg.pdata = (LOG_FAULT_STRUCT *)heap_caps_malloc(sizeof(LOG_FAULT_STRUCT), MALLOC_CAP_SPIRAM); // malloc MALLOC_CAP_SPIRAM
	if (!queue_msg.pdata) 
	{
		ESP_LOGE(TAG, "xQueue_Log_record message and malloc failed");
	} 
	else 
	{
		// ESP_LOGI(TAG, "xCanBusQueue_Recv  ----------222");//testwx
		memcpy(queue_msg.pdata, &reals.log_fault_info, sizeof(reals.log_fault_info));
		queue_msg.INV_NUM = node_id;
		// 消息保存到队列
		if (xQueueSendToBack((QueueHandle_t)xQueue_Log_record, &queue_msg, 0) != pdPASS) 
		{
			ESP_LOGE(TAG, "Inv_Fault_Log_Push message push queue failed");
			free(queue_msg.pdata);
			queue_msg.pdata = NULL;
		}

	}
}

/*
本地事件记录

从已保存到缓存的历史记录队列写入到flash

return:
0-ok
other-fail
*/
uint8_t Logger_Pop_Event(void)// 
{
	LOG_EventHistoryData_Struct queue_msg =  {NULL};
	char fname[30] = {0};
	uint32_t file_bias =0;
	uint8_t rtn=0xFF;

	if(xQueue_iot_historydata_record && xQueueReceive(xQueue_iot_historydata_record, &queue_msg, 0) == pdTRUE)//pdMS_TO_TICKS(task_delay)
	{

	ESP_LOGI(TAG, "Logger_Pop_Event 	");

		LOG_FILE_PATH_IOT(fname,1,EVENT_IOT_PROTOCOL);
		
		file_bias = (uint16_t)SetData.dev_info_t.Event_AddrIndex;// *sizeof(LOG_FAULT_STRUCT);

		if(0 == SetData.dev_info_t.Event_AddrIndex)
		{
//			remove(fname);
//			file_list_clear();//debug
		}
//		if (0 == historic_data_write(fname, (uint8_t *)&reals.log_fault_info, file_bias, sizeof(LOG_FAULT_STRUCT))) 
		if (0 == historic_data_write(fname, (uint8_t *)queue_msg.pdata, file_bias, sizeof(EventHistoryData_Struct))) 
		{
			dump_buf("historic_data_write：：", (uint8_t *)queue_msg.pdata, sizeof(EventHistoryData_Struct));

			 ESP_LOGI(TAG, "File written ok (%s)",fname);
			 rtn =0;

			 if(0 == rtn)	 
			 {
				 SetData.dev_info_t.Event_AddrIndex++;
				 if(SetData.dev_info_t.Event_AddrIndex >= HISTORY_MAX_COUNT)
				 {
					 SetData.dev_info_t.Event_AddrIndex=0;
				 }
				 else
				 {
				 }
			 
				 SetData.dev_info_t.Event_SaveCount++;
				 if(SetData.dev_info_t.Event_SaveCount > HISTORY_MAX_COUNT )	 //100		 
				 {
					 SetData.dev_info_t.Event_SaveCount = HISTORY_MAX_COUNT;
				 }
							 
//				  if((SetData.dev_info_t.historyRecSaveCount < SetData.dev_info_t.historyAddrIndex)
//					  ||((SetData.dev_info_t.historyRecSaveCount > SetData.dev_info_t.historyAddrIndex)&&(HISTORY_MAX_COUNT != SetData.dev_info_t.historyRecSaveCount)))//纠错处理
//				 {
//					 SetData.dev_info_t.historyRecSaveCount=0;
//					 SetData.dev_info_t.historyAddrIndex=0;
//				 }	 
			 
				  reals.SetDataWrFlag.sBit.SetDataUpdate_Event_cnt =1;				 
			 }
			 
		}
		else
		{
			ESP_LOGE(TAG, "File written fail:Logger_Pop_Event (%s)",fname );
		}

	
		if (queue_msg.pdata) 
		{
			free(queue_msg.pdata); /* 处理完成释放CAN数据帧 */
			queue_msg.pdata = NULL; 
	
		}
	
	}
return rtn;
}

/*
实时告警记录存储到队列缓存
*/
void Logger_Push_Event(void)//100ms cycle
{
	

	LOG_EventHistoryData_Struct queue_msg =  {NULL};

	queue_msg.pdata = (EventHistoryData_Struct *)heap_caps_malloc(sizeof(EventHistoryData_Struct), MALLOC_CAP_SPIRAM); // malloc MALLOC_CAP_SPIRAM
	if (!queue_msg.pdata) 
	{
		ESP_LOGE(TAG, "xQueue_Log_record2 message and malloc failed");
	} 
	else 
	{
		// ESP_LOGI(TAG, "xCanBusQueue_Recv  ----------222");//testwx
		memcpy(queue_msg.pdata, &reals.HistoryData_event, sizeof(reals.HistoryData_event));
		// 消息保存到队列
		if (xQueueSendToBack((QueueHandle_t)xQueue_iot_historydata_record, &queue_msg, 0) != pdPASS) 
		{
			ESP_LOGE(TAG, "Logger_Push_Event message push queue failed");
			free(queue_msg.pdata);
			queue_msg.pdata = NULL;
		}

	}




}

/******************************************************************************
HappenType:1-发生；0-消除
ErrorCode:fault/alarm寄存器顺序号
FaultInformation:相关的U16的 value



******************************************************************************/
void SaveOneErrorcode(uint8_t HappenType,uint8_t ErrorCode,uint16_t FaultInformation, uint8_t node_id)
{
	time_t now_date = time(NULL);
	/* 日期转换为时间结构 */
	struct tm tm_now;
//	static uint16_t scnt=0;
	memcpy(&tm_now, localtime(&now_date), sizeof(struct tm));
	/* RTC时间转换为自定义BETA格式时间 */
	if (tm_now.tm_year < 100) 
	{
		reals.rtc_time.year = 0;
	}
	else 
	{
		reals.rtc_time.year = ((uint16_t)tm_now.tm_year + 1900 - 2000);
	}
	reals.log_fault_info.LogTime.year = reals.rtc_time.year;
	reals.log_fault_info.LogTime.mon = tm_now.tm_mon + 1;
	reals.log_fault_info.LogTime.day = tm_now.tm_mday;
	reals.log_fault_info.LogTime.hour = tm_now.tm_hour;
	reals.log_fault_info.LogTime.min = tm_now.tm_min;
	reals.log_fault_info.LogTime.sec = tm_now.tm_sec;

	reals.log_fault_info.LogFaultSeq.FaultState = HappenType;
	reals.log_fault_info.LogFaultSeq.FaultSeq = ErrorCode;
	reals.log_fault_info.LogFaultCode = FaultInformation;
	// reals.log_fault_info.LogFaultCode += 1;
	
	reals.flasWrFlag.sBit.log_fault = 1;
	Inv_Fault_Log_Push(node_id);//历史记录单独存储 -xf
	ESP_LOGI(TAG, "SaveOneErrorcode:%d-%d-%d-%d-%d-%d",reals.log_fault_info.LogTime.year,reals.log_fault_info.LogTime.mon,reals.log_fault_info.LogTime.day,reals.log_fault_info.LogTime.hour,reals.log_fault_info.LogTime.min,reals.log_fault_info.LogTime.sec);
	ESP_LOGI(TAG, "HappenType=%d,ErrorCode=%d, FaultInformation=%d	, node id=%d",HappenType,ErrorCode, FaultInformation, node_id);

}



/*------------------------------------------------------------------------------
 Function: inv_fault_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      处理故障信息
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void inv_fault_check(void)
{  
	uint16_t i=0;
	uint8_t FaultNumber = 0;
	uint16_t dLastErrorCodeTemp    = 0;
	uint16_t dCurrentErrorCodeTemp = 0;
	uint16_t HaveErrorCode = 0;
	uint16_t NothaveErrorCode = 0;
	uint16_t CurrentPCSFault[5]={0};
	// static uint16_t PCSFault[5] = {0};
	static uint16_t PCSFault[3][5] = {0};//三台逆变器历史记录
	
	for(uint8_t j = 0 ; j < DEV_MAIN_NODE_MAX ; j++)
	{
		for(FaultNumber=0;FaultNumber<5;FaultNumber++)
		{
			CurrentPCSFault[FaultNumber] =Inv[j].mod_reg00100_AppPage1.fault[FaultNumber];
			HaveErrorCode = 0;
			NothaveErrorCode = 0;
		
			if(PCSFault[j][FaultNumber]!= CurrentPCSFault[FaultNumber])
			{
				for(i=0;i<16;i++)
				{  
					dLastErrorCodeTemp = PCSFault[j][FaultNumber] & (0x01 << i);
					dCurrentErrorCodeTemp = CurrentPCSFault[FaultNumber] & (0x01 << i);

					if ((dLastErrorCodeTemp == 0) && dCurrentErrorCodeTemp) 
					{
						HaveErrorCode |= dCurrentErrorCodeTemp;  // 新产生故障
	//					  CheckNewFault(FaultNumber,i);//buzzer
					}
					else if (dLastErrorCodeTemp && (dCurrentErrorCodeTemp == 0)) 
					{
						NothaveErrorCode |=  dLastErrorCodeTemp; //消除的故障
					}
				}
				PCSFault[j][FaultNumber]= CurrentPCSFault[FaultNumber];
				if(NothaveErrorCode!=0)
				{
					SaveOneErrorcode(0,FaultNumber+1,NothaveErrorCode,j);
					SaveErrorDetailInfo(0,FaultNumber,NothaveErrorCode,j+1);
				}
				if(HaveErrorCode!=0)
				{
					SaveOneErrorcode(1,FaultNumber+1,HaveErrorCode,j);
					SaveErrorDetailInfo(1,FaultNumber,HaveErrorCode,j+1);				
				}
	//			break;
			}
		}
	}
}


/*------------------------------------------------------------------------------
 Function: inv_achub_fault_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      处理故障信息
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void inv_achub_fault_check(void)
{  
	uint16_t i=0;
	uint8_t FaultNumber = 0;
	uint16_t dLastErrorCodeTemp    = 0;
	uint16_t dCurrentErrorCodeTemp = 0;
	uint16_t HaveErrorCode = 0;
	uint16_t NothaveErrorCode = 0;
	uint16_t CurrentPCSFault[5]={0};
	// static uint16_t PCSFault[5] = {0};
	static uint16_t AchubFault[5] = {0};//achub历史记录
	
	for(FaultNumber=0;FaultNumber<5;FaultNumber++)
	{
		CurrentPCSFault[FaultNumber] =Inv_AcHub.mod_reg00100_AppPage1.fault[FaultNumber];
		//CurrentPCSFault[FaultNumber] =Inv[3].mod_reg12000_IOT_set.testfault;
		HaveErrorCode = 0;
		NothaveErrorCode = 0;
	
		if(AchubFault[FaultNumber]!= CurrentPCSFault[FaultNumber])
		{
			for(i=0;i<16;i++)
			{  
				dLastErrorCodeTemp = AchubFault[FaultNumber] & (0x01 << i);
				dCurrentErrorCodeTemp = CurrentPCSFault[FaultNumber] & (0x01 << i);

				if ((dLastErrorCodeTemp == 0) && dCurrentErrorCodeTemp) 
				{
					HaveErrorCode |= dCurrentErrorCodeTemp;  // 新产生故障
//					  CheckNewFault(FaultNumber,i);//buzzer
				}
				else if (dLastErrorCodeTemp && (dCurrentErrorCodeTemp == 0)) 
				{
					NothaveErrorCode |=  dLastErrorCodeTemp; //消除的故障
				}
			}
			AchubFault[FaultNumber]= CurrentPCSFault[FaultNumber];
			if(NothaveErrorCode!=0)
			{
				SaveOneErrorcode(0,FaultNumber+1,NothaveErrorCode,DEV_MAIN_NODE_MAX+1);
				SaveErrorDetailInfo(0,FaultNumber,NothaveErrorCode,DEV_MAIN_NODE_MAX+1);
			}
			if(HaveErrorCode!=0)
			{
				SaveOneErrorcode(1,FaultNumber+1,HaveErrorCode,DEV_MAIN_NODE_MAX+1);
				SaveErrorDetailInfo(1,FaultNumber,HaveErrorCode,DEV_MAIN_NODE_MAX+1);
			}
//			break;
		}
	}
}

/*------------------------------------------------------------------------------
 Function: inv_alarm_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      处理告警信息
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void inv_alarm_check(void)
{  
	uint16_t i=0;
	uint8_t FaultNumber = 0;
	uint16_t dLastErrorCodeTemp    = 0;
	uint16_t dCurrentErrorCodeTemp = 0;
	uint16_t HaveErrorCode = 0;
	uint16_t NothaveErrorCode = 0;
	uint16_t CurrentPCSFault[5]={0};
	// static uint16_t PCSFault[5] = {0};
	static uint16_t PCSFault[3][5] = {0};//三台逆变器历史记录
	

	for(uint8_t j = 0 ; j < DEV_MAIN_NODE_MAX ; j++)
	{
		for(FaultNumber=0;FaultNumber<4;FaultNumber++)
		{
			CurrentPCSFault[FaultNumber] =Inv[j].mod_reg00100_AppPage1.alarm[FaultNumber];
			//CurrentPCSFault[FaultNumber] =Inv[j].mod_reg12000_IOT_set.testfault;
			//ESP_LOGW(TAG,"Inv[%d].mod_reg00100_AppPage1.alarm[%d]:0x%x",j,FaultNumber,Inv[j].mod_reg00100_AppPage1.alarm[FaultNumber]);
			HaveErrorCode = 0;
			NothaveErrorCode = 0;
			if(PCSFault[j][FaultNumber]!= CurrentPCSFault[FaultNumber])
			{
				for(i=0;i<16;i++)
				{  
					dLastErrorCodeTemp = PCSFault[j][FaultNumber] & (0x01 << i);
					dCurrentErrorCodeTemp = CurrentPCSFault[FaultNumber] & (0x01 << i);

					if ((dLastErrorCodeTemp == 0) && dCurrentErrorCodeTemp) 
					{
						HaveErrorCode |= dCurrentErrorCodeTemp;  // 新产生故障
	//					  CheckNewFault(FaultNumber,i);//buzzer
					}
					else if (dLastErrorCodeTemp && (dCurrentErrorCodeTemp == 0)) 
					{
						NothaveErrorCode |=  dLastErrorCodeTemp; //消除的故障
					}
				}
				PCSFault[j][FaultNumber]= CurrentPCSFault[FaultNumber];
				if(NothaveErrorCode!=0)
				{
					SaveOneErrorcode(0,FaultNumber+7,NothaveErrorCode, j);
					SaveErrorDetailInfo(0,FaultNumber+6,NothaveErrorCode,j+1);
				}
				if(HaveErrorCode!=0)
				{
					SaveOneErrorcode(1,FaultNumber+7,HaveErrorCode, j);
					SaveErrorDetailInfo(1,FaultNumber+6,HaveErrorCode,j+1);
				}
	//			break;
			}
		}
	}
}

/*------------------------------------------------------------------------------
 Function: inv_achub_alarm_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      处理achub告警信息
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void inv_achub_alarm_check(void)
{  
	uint16_t i=0;
	uint8_t AlarmNumber = 0;
	uint16_t dLastErrorCodeTemp    = 0;
	uint16_t dCurrentErrorCodeTemp = 0;
	uint16_t HaveErrorCode = 0;
	uint16_t NothaveErrorCode = 0;
	uint16_t CurrentPCSAlarm[5]={0};
	// static uint16_t PCSFault[5] = {0};
	static uint16_t AchubAlarm[5] = {0};//achub历史记录

	for(AlarmNumber=0;AlarmNumber<4;AlarmNumber++)
	{
		CurrentPCSAlarm[AlarmNumber] =Inv_AcHub.mod_reg00100_AppPage1.alarm[AlarmNumber];
		//CurrentPCSAlarm[AlarmNumber] =Inv[3].mod_reg12000_IOT_set.testfault;
		//ESP_LOGW(TAG,"Inv_AcHub.mod_reg00100_AppPage1.alarm[%d]:0x%x",AlarmNumber,Inv_AcHub.mod_reg00100_AppPage1.alarm[AlarmNumber]);
		HaveErrorCode = 0;
		NothaveErrorCode = 0;
		if(AchubAlarm[AlarmNumber]!= CurrentPCSAlarm[AlarmNumber])
		{
			//ESP_LOGI(TAG,"AchubAlarm[%d]:%u,Inv[3].mod_reg12000_IOT_set.testfault:%u",AlarmNumber,AchubAlarm[AlarmNumber],Inv[3].mod_reg12000_IOT_set.testfault);
			for(i=0;i<16;i++)
			{  
				dLastErrorCodeTemp = AchubAlarm[AlarmNumber] & (0x01 << i);
				dCurrentErrorCodeTemp = CurrentPCSAlarm[AlarmNumber] & (0x01 << i);

				if ((dLastErrorCodeTemp == 0) && dCurrentErrorCodeTemp) 
				{
					HaveErrorCode |= dCurrentErrorCodeTemp;  // 新产生故障
				}
				else if (dLastErrorCodeTemp && (dCurrentErrorCodeTemp == 0)) 
				{
					NothaveErrorCode |=  dLastErrorCodeTemp; //消除的故障
				}
			}
			AchubAlarm[AlarmNumber]= CurrentPCSAlarm[AlarmNumber];
			if(NothaveErrorCode!=0)
			{
				SaveOneErrorcode(0,AlarmNumber+7,NothaveErrorCode, DEV_MAIN_NODE_MAX+1);
				SaveErrorDetailInfo(0,AlarmNumber+6,NothaveErrorCode,DEV_MAIN_NODE_MAX+1);
			}
			if(HaveErrorCode!=0)
			{
				SaveOneErrorcode(1,AlarmNumber+7,HaveErrorCode, DEV_MAIN_NODE_MAX+1);
				SaveErrorDetailInfo(1,AlarmNumber+6,HaveErrorCode,DEV_MAIN_NODE_MAX+1);
			}
//			break;
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

void SaveOneEvent(uint16_t FaultCode,uint16_t FaultOn,uint16_t WarnCode,uint16_t WarnOn)
{
	reals.HistoryData_event.time_s++;// =reals.now;

	reals.HistoryData_event.FaultCode = FaultCode;
	reals.HistoryData_event.FaultOn = FaultOn;
	reals.HistoryData_event.WarnCode = WarnCode;
	reals.HistoryData_event.WarnOn = WarnOn;

    /*发送至队列，等待写入文件系统*/
	Logger_Push_Event();
    reals.flasWrFlag.sBit.iot_event = 1;
    ESP_LOGW(TAG, "SaveOneEvent : FaultCode(%d), FaultOn(%d),WarnCode(%d), WarnOn(%d)", reals.HistoryData_event.FaultCode,reals.HistoryData_event.FaultOn,reals.HistoryData_event.WarnCode,reals.HistoryData_event.WarnOn);
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
void Logger_event_Power_on_off_period(void)
{
	static uint16_t sPoweronflag=0;
	static uint16_t scnt=0;
	
	if(0 == sPoweronflag)
	{
		if(reals.MCUPoweronCnt > 0)//上电10s后记录
		{
			sPoweronflag =1;
            SaveOneEvent(0,0,CODE_ALARM_ON,1);			
		}

	}


	if((1 == reals.st_FlagTime.bits.b1FlagSys1s_slow_period_task)
//		&&(reals.HistoryData_event.time_s <130)
		)
	{
	if(++scnt >= 3600)//1h
	{
		scnt =0;
		SaveOneEvent(0,0,CODE_ALARM_PERIOD_RECORD,1);

	}
	

	}
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
void Logger_event_check(void)
{
    EventHistoryData_Struct HistoryData = {0};

	
//	reals.HistoryData_event.time_s =reals.now;
	reals.HistoryData_event.ver_protocol = EVENT_IOT_PROTOCOL;
	
	reals.HistoryData_event.FaultCode =0;
	reals.HistoryData_event.FaultOn =0;
	reals.HistoryData_event.WarnCode =0;
	reals.HistoryData_event.WarnOn =0;
	reals.HistoryData_event.Num_IOT = reals.online_Iot_num;
	reals.HistoryData_event.Num_INV = reals.online_Inv_num;
	reals.HistoryData_event.Num_PACK = reals.online_Pack_num;
	reals.HistoryData_event.Num_udp_net = reals.Topnet_point_Num;
	reals.HistoryData_event.Num_sub1g_net = 0;//reals.Subnet_point_Num;
	reals.HistoryData_event.time_wifi_connect = 0;//未更新
	reals.HistoryData_event.time_sub1g_connect = 0;//未更新

	Logger_event_Power_on_off_period();
}   

/*向文件系统中写入目标参数*/
int SaveFileileWrite(char* fname,uint8_t* buf ,uint16_t index, uint32_t file_pos,uint16_t writelen)
{
	if(fname == NULL || buf == NULL || writelen == 0) return -1;

	char path[30] = {0};
	SAVE_FILE_PATH(path,fname,0,index);	
	//PARAMETER_FILE_PATH_IOT(fname,0,0);

	//sprintf(path, "%s/%s", RECORD_ROOT_PATH, fname);
  
	//historic_dir_check(fname);
	ESP_LOGI(TAG,"SaveFileileWrite :%s file_pos:%lu writelen:%u",path,file_pos,writelen);
	
	if (0 == SetData_file_data_write(path,buf, file_pos, writelen))
	{
		ESP_LOGI(TAG, "File written ok (%s)",path);
		return 0;
	}
	return -1;
}

/*从文件系统中读取参数*/
int FileReadForName(char* fname,void *buf,uint16_t index,uint32_t file_pos,uint16_t read_size)
{
	if(fname == NULL || buf == NULL || read_size == 0) return -1;

	//uint32_t files_pos=0;
	char path[30] = {0};
	char path2[19] = {0};
	int fd;
	//uint16_t paramLen=sizeof(ll_param_list_t);
	//uint8_t * ubuf=(uint8_t *)_DisasterParam;

	//sprintf(path,"%s/%s%d_%02d",RECORD_ROOT_PATH,fname,0,0);
	SAVE_FILE_PATH(path2,fname,0,index);
	sprintf(path, "%s/%s", RECORD_ROOT_PATH, path2);
	ESP_LOGI(TAG,"FileReadForName:%s read_size:%d",path,read_size);
	fd=open(path,O_RDONLY);
	if(fd<0)
	{
		ESP_LOGE(TAG,"Error: Failed to get disaster_save_param:%s",path);
		return -1;
	}

	uint32_t getLen=lseek(fd,file_pos,SEEK_SET);
	ESP_LOGI(TAG,"DisasterParamListRead : set %lu  ",getLen);
	if(getLen!=file_pos)
	{
		ESP_LOGE(TAG,"Error: disaster_save_param Len err:%lu  %d  ",getLen,read_size);
		return -1;
	}
	if(read(fd, buf, read_size) != read_size) 
	{
		ESP_LOGE(TAG, "[GetHistoryLogFromLittlefs] read error(%s) read_size:%d",path,read_size);
		return -1;
	}

	close(fd);
	return 0;
}

void Default_tou_ctrl_data_init(void)
{
    char fname[30] = {0};
    PARAMETER_FILE_PATH_RELAY(fname,0,0);

    memset(&RelaySetData, 0, sizeof(RelaySetData));
    RelaySetData.Relay_info_t.valid_Relay = TOURELAY_EPROM_READY_FLAG;

    Relay_Ctrl_Factory_Parameter01_Update();
    Relay_Ctrl_Factory_Parameter02_Update();
    Relay_Ctrl_Factory_Parameter03_Update();
    Relay_Ctrl_Factory_Parameter04_Update();

    if (0 == SetData_file_data_write(fname, (uint8_t *)&RelaySetData, 0, sizeof(RelaySetData))) //ok
    {
        ESP_LOGI(TAG, "SetData_Panel File written ok (%s)",fname);
        Relay_ctrl.bit.soc_ctrl_init = 1;
        Relay_ctrl.bit.time_ctrl_init = 1;
        Relay_ctrl.bit.delay_ctrl_init = 1;
        Relay_ctrl.bit.plan_ctrl_init = 1;
    } else {
        ESP_LOGE(TAG, "[get_relay_parameter_from_flash] SetData_Panel File written fail (%s)",fname);
        Relay_ctrl.bit.soc_ctrl_init = 0;
        Relay_ctrl.bit.time_ctrl_init = 0;
        Relay_ctrl.bit.delay_ctrl_init = 0;
        Relay_ctrl.bit.plan_ctrl_init = 0;
    }
}


/*------------------------------------------------------------------------------
 Function: get_relay_parameter_from_flash
 -----------------------------------------------------------------------------*/
/**
  * @brief      读取panel设置参数，注：实际是读取relay设置参数
  * @param[in]  void  
  * @param[out] None
  * @return     int
  */
int get_relay_parameter_from_flash(void)
{
	char path[50] = {0};
	char fname[30] = {0};
	
	int fd;

	PARAMETER_FILE_PATH_RELAY(fname,0,0);
	sprintf(path, "%s/%s", RECORD_ROOT_PATH, fname);

	historic_dir_check(fname);

	/* 只读格式打开文件 */
	fd = open(path, O_RDONLY);
	if(fd < 0)
	{
		ESP_LOGE(TAG, "[get_relay_parameter_from_flash] open error");
		Default_tou_ctrl_data_init();
	}
	else
	{
		/* 移动文件指针到设置的数据位置 */
		if(lseek(fd, 0, SEEK_SET) != 0) 
		{
			ESP_LOGE(TAG, "[get_relay_parameter_from_flash] start_pos set error");
			goto __error;
		}
		
		/* 读数据到缓存 */
		if(read(fd, &RelaySetData, sizeof(RelaySetData)) != sizeof(RelaySetData)) //fail
		{
			ESP_LOGE(TAG, "[get_relay_parameter_from_flash] read error");
			/*读取有误，恢复为默认值*/
			Default_tou_ctrl_data_init();
			goto __error;
		}
		else//ok
		{
			/*判断读取标志位是否合法*/
			if(TOURELAY_EPROM_READY_FLAG != RelaySetData.Relay_info_t.valid_Relay)//参数初始化
			{
				ESP_LOGE(TAG, "[get_relay_parameter_from_flash] valid_panel error");
				Default_tou_ctrl_data_init();
			}
			else
			{
				Relay_ctrl.bit.soc_ctrl_init = 1;
				Relay_ctrl.bit.time_ctrl_init = 1;
				Relay_ctrl.bit.delay_ctrl_init = 1;
				Relay_ctrl.bit.plan_ctrl_init = 1;
				ESP_LOGI(TAG, "SetData File read ok (%s)(total: %d bytes, used: %d bytes)",fname, sizeof(RelaySetData), offsetof(Relay_SetData_TypeDef, Relay_info_t.u8temp));
			}
		}
		close(fd);
	}
	
	return 0;

	__error:
	close(fd);
	return -1;
}


