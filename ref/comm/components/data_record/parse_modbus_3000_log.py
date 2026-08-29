"""
parse_modbus_3000_log.py
解析由 LOG_FAULT_STRUCT (modbus 3000 历史记录) 生成的十六进制转储文件。

每条记录结构（按 C 定义、小端）总长度 10 字节：
 - rtc_time_t: 6 字节 (3 x uint16_t)
     u16[0]: low=mon,   high=year(基于2000)
     u16[1]: low=hour,  high=day
     u16[2]: low=sec,   high=min
 - LogFaultSeq_STRUCT: uint16_t
     low 8 bits: FaultSeq (编号)
     high 8 bits: FaultState (1=发生, 0=清除)
 - LogFaultCode: uint16_t

用法:
  python parse_modbus_3000_log.py
  然后按提示输入包含十六进制字符串的文件路径（或拖放文件名）。
"""
import os
import sys
import struct
import binascii
import datetime
import csv

try:
    import openpyxl
    from openpyxl.styles import Font
    OPENPYXL_AVAILABLE = True
except ImportError:
    OPENPYXL_AVAILABLE = False

RECORD_LEN = 10
RECORD_STRUCT = '<5H'  # 5 * uint16_t = 10 bytes

STATE_MAP = {1: "发生", 0: "清除"}
# 可选：故障/告警码映射（示例，可根据实际扩充）
FAULT_CODE_MAP = {
    0: "无",
    100: "示例故障100",
    200: "示例告警200",
}

CSV_HEADERS = [
    "序号", "时间(可读)", "year", "mon", "day", "hour", "min", "sec",
    "FaultSeq", "FaultState", "FaultCode", "FaultCodeDesc", "raw_timestamp"
]

def _rtc_to_str(u16_0, u16_1, u16_2):
    mon = u16_0 & 0xFF
    year = (u16_0 >> 8) & 0xFF
    hour = u16_1 & 0xFF
    day = (u16_1 >> 8) & 0xFF
    sec = u16_2 & 0xFF
    minute = (u16_2 >> 8) & 0xFF
    year_full = 2000 + year
    try:
        dt = datetime.datetime(year_full, mon, day, hour, minute, sec)
        return dt.strftime('%Y-%m-%d %H:%M:%S'), year_full, mon, day, hour, minute, sec
    except Exception:
        return "无效时间", year_full, mon, day, hour, minute, sec

def parse_modbus_3000_file(filepath):
    if not os.path.exists(filepath):
        print(f"文件不存在: {filepath}")
        return None

    with open(filepath, 'r', encoding='utf-8') as f:
        hexstr = "".join(f.read().split())

    if not hexstr:
        print("文件为空或只包含空白。")
        return None

    try:
        data = binascii.unhexlify(hexstr)
    except (binascii.Error, TypeError) as e:
        print("无效的十六进制内容:", e)
        return None

    length = len(data)
    if length % RECORD_LEN != 0:
        print(f"警告: 文件长度 {length} 不是记录长度 {RECORD_LEN} 的整数倍，末尾将被忽略。")
    n_records = length // RECORD_LEN
    print(f"发现 {n_records} 条记录 (共 {length} 字节)。")

    parsed = []
    for i in range(n_records):
        chunk = data[i*RECORD_LEN:(i+1)*RECORD_LEN]
        u0, u1, u2, seq_all, fault_code = struct.unpack_from(RECORD_STRUCT, chunk)

        time_str, year, mon, day, hour, minute, sec = _rtc_to_str(u0, u1, u2)
        fault_seq = seq_all & 0xFF
        fault_state = (seq_all >> 8) & 0xFF
        fault_state_desc = STATE_MAP.get(fault_state, f"未知({fault_state})")
        fault_code_desc = FAULT_CODE_MAP.get(fault_code, f"未定义({fault_code})")

        record = {
            "序号": i + 1,
            "时间(可读)": time_str,
            "year": year,
            "mon": mon,
            "day": day,
            "hour": hour,
            "min": minute,
            "sec": sec,
            "FaultSeq": fault_seq,
            "FaultState": fault_state_desc,
            "FaultCode": fault_code,
            "FaultCodeDesc": fault_code_desc,
            "raw_timestamp": f"{year:04d}-{mon:02d}-{day:02d} {hour:02d}:{minute:02d}:{sec:02d}"
        }
        parsed.append(record)

    return parsed

def export_csv(records, outpath):
    with open(outpath, 'w', newline='', encoding='utf-8-sig') as f:
        w = csv.writer(f)
        w.writerow(CSV_HEADERS)
        for r in records:
            w.writerow([r[h] for h in CSV_HEADERS])
    print("导出 CSV:", outpath)

def export_xlsx(records, outpath):
    if not OPENPYXL_AVAILABLE:
        print("openpyxl 未安装，无法导出 xlsx。")
        return
    wb = openpyxl.Workbook()
    ws = wb.active
    ws.title = "modbus3000_logs"
    ws.append(CSV_HEADERS)
    for cell in ws[1]:
        cell.font = Font(bold=True)
    for r in records:
        ws.append([r[h] for h in CSV_HEADERS])
    wb.save(outpath)
    print("导出 XLSX:", outpath)

if __name__ == '__main__':
    try:
        infile = input("输入 hex 日志文件名 (例如: modbus3000_dump.txt): ").strip()
        recs = parse_modbus_3000_file(infile)
        if not recs:
            print("无解析结果，退出。")
            sys.exit(0)

        print("\n前 5 条预览：")
        for r in recs[:5]:
            print(f"{r['序号']}: {r['时间(可读)']}, Code={r['FaultCode']} ({r['FaultCodeDesc']}), State={r['FaultState']}")

        choice = input("\n导出为 (1) CSV  (2) XLSX  (Enter 跳过): ").strip()
        base = os.path.splitext(infile)[0]
        if choice == '1':
            export_csv(recs, base + "_parsed_modbus3000.csv")
        elif choice == '2':
            export_xlsx(recs, base + "_parsed_modbus3000.xlsx")
        else:
            print("结束。")

    except KeyboardInterrupt:
        print("\n已取消。")
    except Exception as e:
        print("发生错误:", e)