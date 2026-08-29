"""
@File      : parse_iot_c05.py
@Version   : 1.3
@Brief     : 解析CAN日志二进制/十六进制文件，输出结构化解析结果
@Details   :
  - 输入：支持纯二进制文件、带空格/换行的十六进制文本文件
  - 约束：文件无额外头信息，按log_frame_t结构体连续存储
  - 处理：自动识别文件格式、按结构体解包、解析CAN ID位域、格式化时间戳
  - 输出：控制台预览 + 可选导出为TXT(CSV格式)或Excel文件
  - 适配：修复Windows双击闪退，支持拖拽文件运行，无控制台环境自动回退GUI
"""

# ==============================================================================
# 依赖库导入
# ==============================================================================
import struct
import datetime
import sys
import os
import csv
import binascii

# Windows控制台编码修复：解决双击运行时中文乱码触发的闪退问题
if sys.platform == "win32":
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')
    sys.stdin = io.TextIOWrapper(sys.stdin.buffer, encoding='utf-8', errors='replace')

# 尝试导入Excel导出依赖
try:
    import openpyxl
    from openpyxl.styles import Font
    OPENPYXL_AVAILABLE = True
except ImportError:
    OPENPYXL_AVAILABLE = False

# 尝试导入GUI依赖，用于无控制台场景回退
try:
    import tkinter as tk
    from tkinter import filedialog, messagebox
    TK_AVAILABLE = True
except Exception:
    TK_AVAILABLE = False


# ==============================================================================
# 全局常量定义 (与C结构体log_frame_t严格对应)
# ==============================================================================

# 单条记录固定字节长度：type(1) + len(2) + timestamp(4) + id(4) + data(8) = 19字节
EVENT_RECORD_LEN = 19

# 小端模式结构体解包格式：uint8 + uint16 + uint32 + uint32 + uint8[8]
RECORD_STRUCT_FORMAT = '<BHII8s'

# 日志类型映射表
TYPE_MAP = {
    104: "LOG_TYPE_CAN_TX",
    134: "LOG_TYPE_CAN_RX"
}

# 导出文件表头
CSV_HEADERS = [
    "记录序号", "可读时间", "帧类型", "数据长度",
    "CAN标识符", "保留位", "是否发送", "数据(逐字节HEX)", "原始时间戳"
]


# ==============================================================================
# 核心功能函数
# ==============================================================================

def load_file_content(filepath):
    """
    读取并自动识别文件格式，统一返回二进制字节流
    支持纯二进制文件、带空格/换行的十六进制文本文件
    """
    with open(filepath, 'rb') as f:
        raw_data = f.read()

    if not raw_data:
        raise ValueError("文件内容为空")

    # 尝试识别为十六进制文本
    try:
        txt_content = raw_data.decode('ascii').strip()
        if all(ch.isspace() or ch in '0123456789abcdefABCDEF' for ch in txt_content):
            hex_str = "".join(txt_content.split())
            return binascii.unhexlify(hex_str)
    except Exception:
        pass

    # 识别失败按纯二进制处理
    return raw_data


def parse_log_frame_file(filepath):
    """
    解析CAN日志文件，返回结构化记录列表
    @param filepath: 日志文件路径
    @return: 解析完成的字典列表，失败返回None
    """
    if not os.path.exists(filepath):
        print(f"\n错误: 文件 '{filepath}' 不存在或路径不正确")
        return None

    # 读取并转换文件内容
    try:
        binary_content = load_file_content(filepath)
    except Exception as e:
        print(f"\n读取文件失败: {e}")
        return None

    file_size = len(binary_content)
    print(f"\n--- 开始解析文件: '{filepath}' (有效数据 {file_size} 字节) ---")

    if file_size < EVENT_RECORD_LEN:
        print("文件数据过短，不包含完整记录")
        return []

    if file_size % EVENT_RECORD_LEN != 0:
        print(f"警告: 文件大小不是记录长度的整数倍，末尾不完整数据将被忽略")

    # 循环解析所有记录
    num_records = file_size // EVENT_RECORD_LEN
    parsed_records = []
    print(f"\n--- 共发现 {num_records} 条日志记录 ---")

    for i in range(num_records):
        chunk = binary_content[i * EVENT_RECORD_LEN: (i + 1) * EVENT_RECORD_LEN]
        try:
            frame_type, data_len, timestamp, can_id_raw, data_bytes = \
                struct.unpack_from(RECORD_STRUCT_FORMAT, chunk)
        except struct.error as e:
            print(f"第 {i+1} 条记录解包失败: {e}")
            continue

        # 解析CAN ID位域（与can_id_frame_t联合体对应）
        # identifier: 低29位; revd: 第29~30位; tx: 第31位
        can_identifier = can_id_raw & 0x1FFFFFFF
        revd_bit = (can_id_raw >> 29) & 0x3
        tx_flag = (can_id_raw >> 31) & 0x1

        # 按实际长度截取有效数据，逐字节空格分隔显示
        valid_data = data_bytes[:min(data_len, 8)]
        data_hex_str = " ".join(f"{b:02X}" for b in valid_data)

        # CAN标识符转为十六进制字符串显示
        can_id_hex = f"0x{can_identifier:08X}"

        # 时间戳格式化
        try:
            time_str = datetime.datetime.fromtimestamp(timestamp).strftime('%Y-%m-%d %H:%M:%S')
        except (ValueError, OSError):
            time_str = f"无效时间戳({timestamp})"

        # 组装记录字典
        record = {
            "记录序号": i + 1,
            "可读时间": time_str,
            "帧类型": TYPE_MAP.get(frame_type, f"未知类型({frame_type})"),
            "数据长度": data_len,
            "CAN标识符": can_id_hex,
            "保留位": revd_bit,
            "是否发送": tx_flag,
            "数据(逐字节HEX)": data_hex_str,
            "原始时间戳": timestamp
        }
        parsed_records.append(record)

    print("\n--- 解析完成 ---")
    return parsed_records


def export_to_txt_csv(records, output_filename):
    """导出解析结果为TXT(CSV格式)文件"""
    with open(output_filename, 'w', newline='', encoding='utf-8-sig') as f:
        writer = csv.writer(f)
        writer.writerow(CSV_HEADERS)
        for rec in records:
            writer.writerow([rec[h] for h in CSV_HEADERS])
    print(f"\n结果已成功导出到: {output_filename}")


def export_to_excel(records, output_filename):
    """导出解析结果为格式化Excel文件"""
    if not OPENPYXL_AVAILABLE:
        print("\n错误: 未安装openpyxl库，无法导出Excel")
        print("请执行: pip install openpyxl")
        return

    wb = openpyxl.Workbook()
    ws = wb.active
    ws.title = "CAN日志记录"
    ws.append(CSV_HEADERS)

    # 表头加粗
    for cell in ws[1]:
        cell.font = Font(bold=True)

    # 写入数据
    for rec in records:
        ws.append([rec[h] for h in CSV_HEADERS])

    # 自适应列宽
    for col in ws.columns:
        max_len = 0
        col_letter = col[0].column_letter
        for cell in col:
            try:
                if len(str(cell.value)) > max_len:
                    max_len = len(str(cell.value))
            except Exception:
                pass
        ws.column_dimensions[col_letter].width = max_len + 2

    wb.save(output_filename)
    print(f"\n结果已成功导出到: {output_filename}")


# ==============================================================================
# GUI辅助与运行适配
# ==============================================================================

_root_window = None

def _get_tk_root():
    """获取全局隐藏的Tk根窗口实例"""
    global _root_window
    if not TK_AVAILABLE:
        return None
    if _root_window is None:
        _root_window = tk.Tk()
        _root_window.withdraw()
    return _root_window


def ask_file_via_dialog():
    """弹出文件选择对话框"""
    root = _get_tk_root()
    if not root:
        return None
    path = filedialog.askopenfilename(
        title="选择要解析的日志文件",
        filetypes=[("日志文件", "*.bin *.txt *.log"), ("所有文件", "*.*")]
    )
    return path if path else None


def show_info_box(title, msg):
    """弹出消息提示框，无GUI环境则打印到控制台"""
    root = _get_tk_root()
    if root:
        try:
            messagebox.showinfo(title, msg)
            return
        except Exception:
            pass
    print(f"\n[{title}] {msg}")


def wait_exit(prompt="按回车键退出..."):
    """程序结束等待，防止双击运行窗口一闪而过"""
    try:
        if sys.stdin and sys.stdin.isatty():
            input(f"\n{prompt}")
            return
    except Exception:
        pass

    try:
        show_info_box("程序结束", prompt)
        return
    except Exception:
        pass

    if sys.platform == "win32":
        os.system("pause")


# ==============================================================================
# 主程序入口
# ==============================================================================

if __name__ == "__main__":
    try:
        input_path = ""

        # 1. 优先处理拖拽/命令行传参
        if len(sys.argv) > 1:
            input_path = sys.argv[1]
        # 2. 双击运行且有GUI能力时，优先弹出文件选择框
        elif TK_AVAILABLE:
            input_path = ask_file_via_dialog()
        # 3. 纯控制台模式下手动输入路径
        else:
            input_path = input("请输入要解析的日志文件路径: ").strip()

        if not input_path:
            print("未选择文件，程序退出")
            wait_exit()
            sys.exit(0)

        # 执行解析
        records_list = parse_log_frame_file(input_path)

        if records_list is None:
            wait_exit()
            sys.exit(1)

        if records_list:
            # 打印前5条预览
            print("\n--- 解析结果预览(前5条) ---")
            for rec in records_list[:5]:
                print(f"#{rec['记录序号']:04d} | {rec['可读时间']} | {rec['帧类型']} | ID:{rec['CAN标识符']} | 数据: {rec['数据(逐字节HEX)']}")
            print("-" * 80)

            base_name = os.path.splitext(input_path)[0]

            # 导出格式选择
            while True:
                print("\n请选择输出格式:")
                print("  1 - TXT(CSV格式)")
                print("  2 - Excel文件")
                print("  直接回车 - 不导出退出")

                if sys.stdin and sys.stdin.isatty():
                    choice = input("请输入选项: ").strip()
                elif TK_AVAILABLE:
                    if messagebox.askyesno("导出结果", "是否导出为CSV文件？"):
                        choice = "1"
                    elif messagebox.askyesno("导出结果", "是否导出为Excel文件？"):
                        choice = "2"
                    else:
                        choice = ""
                else:
                    choice = ""

                if choice == "1":
                    export_to_txt_csv(records_list, f"{base_name}_parsed.txt")
                    break
                elif choice == "2":
                    export_to_excel(records_list, f"{base_name}_parsed.xlsx")
                    break
                elif choice == "":
                    print("\n未选择导出，程序结束")
                    break
                else:
                    print("无效输入，请输入 1、2 或直接回车")
        else:
            print("\n未解析到任何有效记录")

    except KeyboardInterrupt:
        print("\n\n操作已由用户取消")
    except Exception as e:
        error_msg = f"程序运行发生错误: {str(e)}"
        print(f"\n{error_msg}")
        try:
            show_info_box("运行错误", error_msg)
        except Exception:
            pass
    finally:
        wait_exit()