"""
解析由 Inv_Detailed_Info_Datas (BOX_LOG_MAX_LEN = 208) 连续组成的二进制文件。
每条记录 = 208 字节 = pre_DetailedInfo(104) + cur_DetailedInfo(104)。

用法: python parse_box_log.py
"""
import os
import struct
import csv
import datetime
import sys

try:
    import openpyxl
    from openpyxl.styles import Font
    OPENPYXL = True
except ImportError:
    OPENPYXL = False

RECORD_LEN = 208
SINGLE_INFO_FORMAT = '<I' + 'H'*9 + 'hhhh' + 'H'*22 + '15H'
SINGLE_INFO_SIZE = struct.calcsize(SINGLE_INFO_FORMAT)
assert SINGLE_INFO_SIZE * 2 == RECORD_LEN

# 字段顺序（注意加入 alarmState 用于表示告警发生/消除）
INFO_FIELDS = [
    "curTime",
    "ver", "alarmCode", "alarmState", "invWorkState", "setCtrlWorkMode", "gridFreq",
    "grid1Voltage","grid1Current","grid2Voltage","grid2Current",
    "inv1Voltage","inv1Current","inv2Voltage","inv2Current",
    "acLoad1Voltage","acLoad1Current","acLoad2Voltage","acLoad2Current",
    "pv1Voltage","pv1Current","pv2Voltage","pv2Current",
    "ambientTemp","invMaxTemp","pvDcdcMaxTemp","device_seq",
    "packTotalVoltage","packTotalCurrent","soc","soh","packRunStatus",
    "cellMinVoltage","cellMaxVoltage","cellMinTemp","cellMaxTemp","packCycle"
]
for i in range(1, 16):
    INFO_FIELDS.append(f"reg_{i:02d}")

# 中文表头映射（与 INFO_FIELDS 顺序一一对应）
CHN_FIELDS = [
    "时间戳",
    "协议版本", "告警码", "告警状态", "逆变工作状态","逆变工作模式","电网频率",
    "电网1电压(V)","电网1电流(A)","电网2电压(V)","电网2电流(A)",
    "逆变1电压(V)","逆变1电流(A)","逆变2电压(V)","逆变2电流(A)",
    "交流负载1电压(V)","交流负载1电流(A)","交流负载2电压(V)","交流负载2电流(A)",
    "PV1电压(V)","PV1电流(A)","PV2电压(V)","PV2电流(A)",
    "环境温度(℃)","逆变器最高温度(℃)","PV DCDC最高温度(℃)","设备序号(原始)",
    "Pack总电压(V)","Pack总电流(A)","SOC(%)","SOH(%)","Pack运行状态",
    "电芯最小电压(V)","电芯最大电压(V)","电芯最小温度(℃)","电芯最大温度(℃)","Pack循环次数"
]
for i in range(1, 16):
    CHN_FIELDS.append(f"透传寄存器{i:02d}")

# 最终 CSV/Excel 表头：记录索引、部位(pre/cur) + 中文字段 + 并机序号两列
CSV_HEADERS = ["记录索引", "部位"] + CHN_FIELDS + ["并机逆变器序号", "并机电池包序号"]

def read_file_auto(path):
    if not os.path.exists(path):
        raise FileNotFoundError(path)
    with open(path, "rb") as f:
        data = f.read()
    # 尝试识别并解码 ASCII hex 文本
    try:
        txt = data.decode('ascii', errors='ignore').strip()
        hexchars = set("0123456789abcdefABCDEF\r\n\t ")
        if txt and all(c in hexchars for c in txt):
            hexstr = "".join(txt.split())
            if len(hexstr) >= 2:
                try:
                    return bytes.fromhex(hexstr)
                except Exception:
                    pass
    except Exception:
        pass
    return data

def decode_device_seq(u16):
    inv_seq = u16 & 0xFF
    pack_seq = (u16 >> 8) & 0xFF
    return inv_seq, pack_seq

def parse_single_info(buf):
    if len(buf) != SINGLE_INFO_SIZE:
        raise struct.error(f"buffer size {len(buf)} != {SINGLE_INFO_SIZE}")
    vals = struct.unpack(SINGLE_INFO_FORMAT, buf)
    d = {}
    d["curTime"] = vals[0]
    # 依次填充除了 curTime 之外的字段（注意 INFO_FIELDS[1:] 包含 alarmCode 和 alarmState 占位）
    # vals 对应位置不包含 alarmState（alarmState 从 alarmCode 的最高位解码得到）
    # 因此先把 unpack 的值按原顺序填入一个临时列表，再处理 alarmCode
    unpack_fields = INFO_FIELDS[1:]  # 包含 alarmCode, alarmState, ...
    # 由于 alarmState 在 struct 中不存在，我们需要在填充时跳过 alarmState 插槽
    val_iter = iter(vals[1:])
    for name in unpack_fields:
        if name == "alarmState":
            # 先占位，后面由 alarmCode 解码
            d[name] = None
        else:
            d[name] = next(val_iter)
    # 解析 alarmCode 的最高位为状态，低15位为实际告警码
    raw_alarm = d.get("alarmCode", 0)
    alarm_state = (raw_alarm >> 15) & 0x1
    alarm_code = raw_alarm & 0x7FFF
    d["alarmState"] = alarm_state
    d["alarmCode"] = alarm_code
    # 解析 device_seq 并机序号
    inv_seq, pack_seq = decode_device_seq(d["device_seq"])
    d["device_inv_seq"] = inv_seq
    d["device_pack_seq"] = pack_seq
    return d

def format_value(field, v):
    if v is None:
        return ""
    # 告警状态字段
    if field == "alarmState":
        return "发生" if int(v) == 1 else "清除"
    # 时间戳格式化
    if field == "curTime":
        try:
            return datetime.datetime.fromtimestamp(int(v)).strftime("%Y-%m-%d %H:%M:%S")
        except Exception:
            return str(v)
    # 电压按 0.1V 单位显示（电芯电压除外）
    if field.endswith("Voltage") and field not in ("cellMinVoltage","cellMaxVoltage"):
        return f"{int(v)/10.0:.1f}"
    if field in ("packTotalVoltage",):
        return f"{int(v)/10.0:.1f}"
    # 电流按 0.1A 单位
    if field.endswith("Current"):
        return f"{int(v)/10.0:.1f}"
    # 电芯电压按 0.001V
    if field in ("cellMinVoltage","cellMaxVoltage"):
        return f"{int(v)/1000.0:.3f}"
    # 温度直接显示
    if field in ("ambientTemp","invMaxTemp","pvDcdcMaxTemp","cellMinTemp","cellMaxTemp"):
        return str(v)
    return str(v)

def parse_file(path):
    data = read_file_auto(path)
    size = len(data)
    if size == 0:
        return []
    if size % RECORD_LEN != 0:
        print(f"警告: 文件大小 {size} 不是 {RECORD_LEN} 的整数倍，末尾将被忽略。")
    count = size // RECORD_LEN
    rows = []
    for rec_idx in range(count):
        rec = data[rec_idx*RECORD_LEN:(rec_idx+1)*RECORD_LEN]
        pre_buf = rec[:SINGLE_INFO_SIZE]
        cur_buf = rec[SINGLE_INFO_SIZE:]
        try:
            pre = parse_single_info(pre_buf)
            cur = parse_single_info(cur_buf)
        except struct.error as e:
            print(f"解析第 {rec_idx} 条记录失败: {e}")
            continue
        # 将 pre 和 cur 各作为一行，部位字段为 "pre"/"cur"
        for part_name, part in (("pre", pre), ("cur", cur)):
            row = [rec_idx, part_name]
            for f in INFO_FIELDS:
                row.append(format_value(f, part.get(f)))
            row.append(str(part.get("device_inv_seq")))
            row.append(str(part.get("device_pack_seq")))
            rows.append(row)
    return rows

def export_csv(rows, outpath):
    if not rows:
        print("无数据可导出")
        return
    with open(outpath, "w", newline="", encoding="utf-8-sig") as f:
        w = csv.writer(f)
        w.writerow(CSV_HEADERS)
        w.writerows(rows)
    print("导出 CSV:", outpath)

def export_excel(rows, outpath):
    if not OPENPYXL:
        print("openpyxl 未安装，无法导出 Excel。")
        return
    from openpyxl import Workbook
    wb = Workbook()
    ws = wb.active
    ws.title = "InvDetailed"
    ws.append(CSV_HEADERS)
    for r in rows:
        ws.append(r)
    for cell in ws[1]:
        cell.font = Font(bold=True)
    for col in ws.columns:
        max_len = 0
        col_letter = col[0].column_letter
        for cell in col:
            try:
                l = len(str(cell.value))
                if l > max_len: max_len = l
            except:
                pass
        ws.column_dimensions[col_letter].width = max(10, max_len+2)
    wb.save(outpath)
    print("导出 Excel:", outpath)

def main():
    fp = input("请输入要解析的黑匣子二进制或十六进制文本文件路径 (例如: box_log.bin or box_log.txt): ").strip()
    if not fp:
        print("未提供文件路径，退出")
        return
    try:
        rows = parse_file(fp)
    except Exception as e:
        print("解析失败:", e)
        return
    print(f"\n--- 解析完成, 输出行数: {len(rows)} (每条记录产生两行: pre/cur) ---")
    # 预览前5行
    print("\n--- 解析结果预览 (前5行) ---")
    for i, row in enumerate(rows[:5]):
        print(f"行 #{i+1}: 记录索引={row[0]}, 部位={row[1]}, 时间={row[2]}")
    print("-" * 30)
    while True:
        print("\n请选择输出格式:")
        print("  1: TXT (CSV 格式)")
        print("  2: Excel (.xlsx)")
        choice = input("请输入选项 (1 或 2，或直接回车跳过): ").strip()
        base = os.path.splitext(fp)[0]
        if choice == '1':
            out = f"{base}_inv_detailed.csv"
            export_csv(rows, out)
            break
        elif choice == '2':
            out = f"{base}_inv_detailed.xlsx"
            export_excel(rows, out)
            break
        elif choice == '':
            print("\n未选择导出，程序结束。")
            break
        else:
            print("无效输入，请输入 1, 2 或直接回车。")
    print("完成。")

if __name__ == "__main__":
    main()
