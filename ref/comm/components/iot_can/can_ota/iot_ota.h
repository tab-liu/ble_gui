#ifndef IOT_OTA_H
#define IOT_OTA_H

#include "stdint.h"
//#include "filesystem.h"
#include "can_transmit.h"
#include "can_data.h"
#include "crc.h"

#ifndef CAN_ID                                  //M106  ((page & 0x04) << 24) |
#define CAN_ID(pro, page, pf, dst, src)       ((uint32_t)(pro<<26) | \
                                                        ((page & 0x03) << 24) | \
                                                        ((pf & 0xFF)<<16) | \
                                                        ((dst & 0xFF)<<8) | \
                                                        (src & 0xFF))
#endif




#define FILE_NAME_FORMAT    "%s_%u_%02u.bin"


enum
{
    OTA_READY=0x70,
    OTA_START = 0x71,
    OTA_START_ACK = 0x72,
    OTA_BLOCK_START = 0x73,
    OTA_BLOCK_DATA = 0x74,
    OTA_BLOCK_DATA_ACK = 0x75,
    OTA_END = 0x76,
    OTA_END_ACK = 0x77,
    OTA_PCT = 0x78,
    OTA_IN_BOOT = 0x79,
};

typedef enum
{
    DATA_NORMAL = 0,
    DATA_RESEND,
    DATA_EXIT,
}FileStatus;

typedef enum
{
    OTA_ERR_NORMAL = 0,
    OTA_ERR_SIZE = 1,
    OTA_ERR_FLASH_ERASE = 2,
    OTA_ERR_FLASH_WRITE = 3,
    OTA_ERR_FLASH_READ = 4,
    OTA_ERR_FILE = 5,
    OTA_ERR_CRC16 = 6,
    OTA_ERR_CRC32 = 7,
    OTA_ERR_SEQ = 8,
    OTA_ERR_TIMEOUT = 9,
    OTA_ERR_NO_RESP = 10,
    OTA_ERR_MEMONY = 11,
    OTA_ERR_ABORT = 12,
    OTA_ERR_FILE_TYPE = 13,
    OTA_ERR_CAN_BUSY = 14, /* can 总线繁忙 */
    OTA_ERR_CAN_ERR = 15, /* can 总线发送失败 */
    OTA_ERR_FILE_NULL = 16,
}ERRCode;

typedef enum
{
    OTA_IDLE = 0,
    OTA_DOING ,
}OTA_STATE;

typedef enum {
    IOT_TO_DEV = 1,
    DEV_TO_SUB = 2,
}PCT_STEP;


#define CAN_OTA_IDLE    0
#define CAN_OTA_IOT     1
#define CAN_OTA_DEV     2
#define CAN_OTA_SILENT  3 // 保持静默

#pragma pack(1)
typedef struct
{
   uint8_t  fileType;
   uint32_t fileVersion;
   uint16_t fileSize;
   uint8_t  nc;
}OtaReady;// 0x70

typedef struct
{
   uint8_t  fileType;
   uint32_t fileVersion;
   uint16_t fileSize;	// 单位为Kbyte
   uint8_t dev_id;
}OtaStart;// 0x71


typedef struct
{
    uint16_t blockSeq;
    uint16_t frameTotal;
    uint16_t blockCrc16;
    uint8_t  retryCnt;
    uint8_t  nc;
}OtaBlock;// 0x73

typedef struct
{
    uint16_t frameSeq;
    uint8_t  bytes[6];
}OtaData;// 0x74


typedef struct
{
    uint8_t  fileType;
    uint32_t fileCrc32;
    uint8_t  nc[3];
}OtaEnd;// 0x76


typedef struct {
	uint8_t fileType;
	uint32_t version;
    uint16_t file_size;
	uint8_t nc;
}OtaBoot;

typedef struct {
    uint8_t statuscode;
    uint8_t errcode;
    uint8_t pct;
    uint8_t depth;
    uint8_t nc[4];
}OtaEndAck;// 0x77

typedef struct {
    uint8_t type;
    uint8_t pct;
    uint8_t status;
    uint8_t errcode;
    uint8_t revd[4];
}OtaPct;

typedef struct
{
    uint8_t isOta;      // 0: 未进行OTA; 1: 正在升级
    uint8_t waitAck;    // 设备是否应答
    uint8_t where;      // 文件传输位置
    uint8_t type;       // 文件类型
    uint8_t errCode;    // 错误码
    uint8_t pct;        // 文件传输进度
    uint8_t level;
}OtaDevice;

#define	 NUM_MAX_CAN_OTA_DEVICE_STATUS	0xFF//
#define	 BIAS_IOT_CAN_OTA_DEVICE_STATUS	0//
#define	 BIAS_INV_CAN_OTA_DEVICE_STATUS	3//
#define	 BIAS_PACK_CAN_OTA_DEVICE_STATUS	6//

#define CAN_OTA_PERIOD_TIMEOUT  6000    //6s

typedef struct {
    OtaStart start;
    OtaBlock block;
    OtaEnd   end;
    OtaPct   pct_78H;   // 0x78给底层传进度用于静默部分设备	
    uint8_t *pblock_buf; /* 数据块指针 */
    uint16_t pblock_len;  /* 数据块长度 */
    uint32_t offset; /* 文件便宜量 */
    uint8_t main_step; /* 主步骤 */
    uint8_t sub_step; /* 子步骤 */
    uint8_t resendCnt; /* 重发次数 */
    uint8_t devCnt; // 记录有几个设备应答
    uint16_t timecnt;
    uint8_t ota_doing; /* 指示can-ota状态，0: 空闲，1：ota正在进行 */
    uint8_t fileOK;
    uint32_t crc32;
    OtaDevice devStatus[NUM_MAX_CAN_OTA_DEVICE_STATUS]; // 记录设备升级状态，用于上报给上层应用,基于CAN 源地址 255个填充;windy 2024 1129精简RAM：只存储部分有限关联设备
//    char curr_name[255];
    FILE *pfile;
    FILE *sender_pfile;

	uint8_t sender_work;//1-CAN OTA 执行；0-结束
	uint8_t sender_type;//	type:OtaFileType ,modbus 701
	uint16_t sender_target_id;//CAN ID OTA 目的地址
	uint32_t sender_main_ver;
	uint8_t retry_max_count;
	uint8_t can_busy;
	int period_cmd_delay;//非零表示正在OTA
	OtaDevice ota_summary;
	uint8_t *ota_data;
    uint16_t ready_timeout;
    bool is_only_stage;
}CanOtaStruct;
extern CanOtaStruct can_ota_status;

#pragma pack()


#define CAN_PAYLOAD_LEN     6


#define CanBusCrc16     xm_calcu_crc16 //calcu_crc16
#define CanBusCrc32     calcu_crc32

// 1: Successul  0: Failed
#define    OTA_SendMessage(node_id, id, msg, len)    CanAckData(node_id, id, msg, len)

extern uint16_t can_ota_pct;
extern uint16_t can_ota_type;
extern uint16_t can_ota_level;

void CanOtaSender_Begin(uint8_t id, uint8_t type, uint32_t version, CanOtaStruct *ota_status);

void CanOtaReceiver(uint8_t node_id, const twai_message_t *frame, CanOtaStruct *ota_status);
void CanOtaSender(uint8_t node_id, const twai_message_t *frame, CanOtaStruct *ota_status);
uint8_t ota_file_check(FILE *pfile);


esp_err_t iot_ota_begin(void);
esp_err_t iot_ota_write(uint8_t *ota_write_data, uint32_t write_len);
esp_err_t iot_ota_end(void);
esp_err_t iot_ota_abort(void);
esp_err_t iot_ota_rollback(void);
#endif
