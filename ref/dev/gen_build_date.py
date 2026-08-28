import sys
import datetime

now = datetime.datetime.now()
local_ts = int(now.timestamp())
local_iso = now.strftime('%Y-%m-%dT%H:%M:%S')

with open(sys.argv[1], "w") as f:
    f.write("#ifndef __BUILD_DATE_H__\n")
    f.write("#define __BUILD_DATE_H__\n\n")
    f.write(f"#define BUILD_DATE_NUM {now.strftime('%Y%m%d')}\n")
    f.write(f"#define BUILD_TIME_NUM {now.strftime('%H%M%S')}\n")
    f.write(f"#define BUILD_TIMESTAMP {now.strftime('%y%m%d%H%M')}\n")
    f.write(f"#define BUILD_DATE_NUM_STR \"{now.strftime('%Y%m%d')}\"\n")
    f.write(f"#define BUILD_TIME_NUM_STR \"{now.strftime('%H%M%S')}\"\n")
    f.write(f"#define BUILD_TIMESTAMP_STR \"{now.strftime('%y%m%d%H%M')}\"\n")
    f.write(f"#define BUILD_LOCAL_TS_SEC {local_ts}\n")
    f.write(f"#define BUILD_LOCAL_TS_SEC_STR \"{local_ts}\"\n")
    f.write(f"#define BUILD_LOCAL_ISO_STR \"{local_iso}\"\n")
    f.write("\n#endif // __BUILD_DATE_H__\n")