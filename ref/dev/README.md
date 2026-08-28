#define HOST_CFG        "HOSTCFG"
#define DEV_ABOUT       "DevAbout"
#define IOT_ABOUT       "IOTAbout"
#define MESH_CFG        "MESHID"


#define BASE_ARG                    FLASH_BASE_PATH"/base_set.txt" // 基本参数设置表
#define ADVANCED_ARG                FLASH_BASE_PATH"/advanced_set.txt" // 高级参数设置表
#define ADJ_ARG                     FLASH_BASE_PATH"/adj_set.txt" // 校准参数表
#define EXTEND_ARG                  FLASH_BASE_PATH"/extend_set.txt" 
#define EMS_SET_AGR                 FLASH_BASE_PATH"/ems_set.txt"  // DCDC独有设置参数，用于EMS管理，时间段，SOC等
#define SHARE_AGR                   FLASH_BASE_PATH"/share_set.txt"  // DCDC和微逆共有参数


#define CH_CQC_SAFETY_ARG           FLASH_BASE_PATH"/cqc_safety_standard.txt"       // 中国认证数据表
#define GER_CDE0126_SAFETY_ARG      FLASH_BASE_PATH"/cde0126_safety_standard.txt"   // 德国认证数据表
#define ER_G83_SAFETY_ARG           FLASH_BASE_PATH"/g83_safety_standard.txt"       // 英国认证数据表
#define AU_AS4777_2_SAFETY_ARG      FLASH_BASE_PATH"/as4777_2_safety_standard.txt"  // 澳大利亚认证数据表
#define ES_RD1663_SAFETY_ARG        FLASH_BASE_PATH"/rd1663_safety_standard.txt"    // 西班牙认证数据表

// 参数备份，防止参数出现错误
#define BASE_ARG_BK                    FLASH_BASE_PATH"/base_set_bk.txt" // 基本参数设置表
#define ADVANCED_ARG_BK                FLASH_BASE_PATH"/advanced_set_bk.txt" // 高级参数设置表
#define ADJ_ARG_BK                     FLASH_BASE_PATH"/adj_set_bk.txt" // 校准参数表
#define EXTEND_ARG_BK                  FLASH_BASE_PATH"/extend_set_bk.txt" 
#define EMS_SET_AGR_BK                 FLASH_BASE_PATH"/ems_set_bk.txt"  // DCDC独有设置参数，用于EMS管理，时间段，SOC等
#define SHARE_AGR_BK                   FLASH_BASE_PATH"/share_set_bk.txt"  // DCDC和微逆共有参数

#define CH_CQC_SAFETY_ARG_BK           FLASH_BASE_PATH"/cqc_safety_standard_bk.txt"       // 中国认证数据表
#define GER_CDE0126_SAFETY_ARG_BK      FLASH_BASE_PATH"/cde0126_safety_standard_bk.txt"   // 德国认证数据表
#define ER_G83_SAFETY_ARG_BK           FLASH_BASE_PATH"/g83_safety_standard_bk.txt"       // 英国认证数据表
#define AU_AS4777_2_SAFETY_ARG_BK      FLASH_BASE_PATH"/as4777_2_safety_standard_bk.txt"  // 澳大利亚认证数据表
#define ES_RD1663_SAFETY_ARG_BK        FLASH_BASE_PATH"/rd1663_safety_standard_bk.txt"    // 西班牙认证数据表

系统中日志标签列表：
app-main    主函数这种运行的状态信息日志标签
app-log     系统事件记录日志
app-wifi    
wifi-lite   
app-uart1
app-uart2
app-set
app-rtc
app-mqtt
app-json
app-http
app-flash
app-erergy
app-data
app-sntp




