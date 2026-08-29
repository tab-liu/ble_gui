#!/usr/bin/env python3
"""
ESP32 UDP接收与智能分析脚本
用于接收ESP32通过WiFi发送的UDP数据，并支持Claude AI分析
"""

import socket
import json
import datetime
import os
from pathlib import Path
import argparse
import sys

# 配置参数
CONFIG = {
    'listen_ip': '0.0.0.0',      # 监听所有网络接口
    'listen_port': 8888,          # UDP监听端口
    'buffer_size': 1024,          # 接收缓冲区大小
    'log_dir': './udp_logs',      # 日志目录
    'claude_api_key': None,       # Claude API密钥（可选）
    'enable_claude': False,       # 是否启用Claude分析
}

class UDPReceiver:
    def __init__(self, config):
        self.config = config
        self.log_dir = Path(config['log_dir'])
        self.log_dir.mkdir(exist_ok=True)

        # 解析来源过滤器（若有）
        sf = config.get('source_filter')
        if sf and isinstance(sf, dict):
            self.source_filter = (sf.get('ip'), sf.get('port'))  # (ip or None, port or None)
        else:
            self.source_filter = None

        # 创建日志文件
        timestamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
        self.log_file = self.log_dir / f'udp_log_{timestamp}.txt'

        # 初始化统计信息
        self.stats = {
            'total_packets': 0,
            'total_bytes': 0,
            'start_time': datetime.datetime.now()
        }

        # 上次发送方（用于只在发送方变化时打印来源）
        self.last_source = None
        # 上次接收时间（用于合并快速连续的行）
        self.last_timestamp = None

    def start(self):
        """启动UDP接收器"""
        print(f"[INFO] 启动UDP接收器...")
        print(f"[INFO] 监听地址: {self.config['listen_ip']}:{self.config['listen_port']}")
        if self.source_filter:
            print(f"[INFO] 仅接收来源: {self.source_filter[0] or '*'}:{self.source_filter[1] or '*'}")        
        print(f"[INFO] 日志文件: {self.log_file}")
        print(f"[INFO] 按 Ctrl+C 停止接收\n")

        try:
            # 创建UDP socket
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.bind((self.config['listen_ip'], self.config['listen_port']))
            sock.settimeout(1.0)  # <- 关键：定期超时以响应 Ctrl+C

            print("[READY] 等待接收UDP数据...\n")

            while True:
                try:
                    # 接收数据
                    data, addr = sock.recvfrom(self.config['buffer_size'])

                    # 如果设置了来源过滤，忽略不匹配的包
                    if self.source_filter:
                        ip_filter, port_filter = self.source_filter
                        if ip_filter and ip_filter != addr[0]:
                            continue
                        if port_filter and port_filter != addr[1]:
                            continue

                    # 解码数据
                    message = data.decode('utf-8', errors='ignore')

                    # 记录接收信息
                    now = datetime.datetime.now()
                    timestamp = now.strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
                    self.stats['total_packets'] += 1
                    self.stats['total_bytes'] += len(data)

                    # 决定是否打印时间与来源：
                    # 当来源与上次不同或距离上次接收超过200ms 时，打印时间+来源+内容；
                    # 否则仅打印数据内容（合并快速连续的行）
                    source = (addr[0], addr[1])
                    delta_ms = None
                    if self.last_timestamp is not None:
                        delta_ms = (now - self.last_timestamp).total_seconds() * 1000.0

                    show_header = (source != self.last_source) or (delta_ms is None) or (delta_ms > 200.0)

                    if show_header:
                        print(f"[{timestamp}] 从 {addr[0]}:{addr[1]}")
                        self.last_source = source

                    # 更新上次接收时间
                    self.last_timestamp = now

                    # 如果 message 已以换行结尾，避免 print 再追加一个换行导致空行
                    if message.endswith('\r\n') or message.endswith('\n') or message.endswith('\r'):
                        print(message, end='')
                    else:
                        print(message)

                    # 写入日志文件
                    log_entry = {
                        'timestamp': timestamp,
                        'source_ip': addr[0],
                        'source_port': addr[1],
                        'data_length': len(data),
                        'data': message
                    }
                    self.write_log(log_entry)

                    # 如果启用Claude分析
                    if self.config['enable_claude'] and self.config['claude_api_key']:
                        self.analyze_with_claude(message, timestamp)

                except socket.timeout:
                    continue
                except KeyboardInterrupt:
                    print("\n\n[INFO] 接收到停止信号")
                    self.stop()                    
                except Exception as e:
                    print(f"[ERROR] 接收数据时出错: {e}")
        except KeyboardInterrupt:
            print("\n\n[INFO] 接收到停止信号")
            self.stop()
        except Exception as e:
            print(f"[ERROR] 启动接收器失败: {e}")
        finally:
            sock.close()

    def write_log(self, log_entry):
        """写入日志文件"""
        try:
            with open(self.log_file, 'a', encoding='utf-8') as f:
                f.write(json.dumps(log_entry, ensure_ascii=False) + '\n')
        except Exception as e:
            print(f"[ERROR] 写入日志失败: {e}")

    def analyze_with_claude(self, message, timestamp):
        """使用Claude API分析数据（需要API密钥）"""
        try:
            import anthropic

            client = anthropic.Anthropic(api_key=self.config['claude_api_key'])

            prompt = f"""请分析以下ESP32传感器数据，提供简明扼要的见解：

接收时间: {timestamp}
数据内容: {message}

请分析：
1. 数据的含义和用途
2. 是否存在异常值
3. 可能的应用场景
4. 任何值得关注的信息

请用简洁的语言回复。"""

            response = client.messages.create(
                model="claude-3-5-sonnet-20241022",
                max_tokens=500,
                messages=[{"role": "user", "content": prompt}]
            )

            analysis = response.content[0].text
            print(f"\n[Claude分析] {analysis}\n")

            # 保存分析结果
            analysis_file = self.log_dir / 'claude_analysis.txt'
            with open(analysis_file, 'a', encoding='utf-8') as f:
                f.write(f"\n{'='*60}\n")
                f.write(f"时间: {timestamp}\n")
                f.write(f"数据: {message}\n")
                f.write(f"分析:\n{analysis}\n")

        except ImportError:
            print("[WARN] 未安装anthropic库，无法使用Claude分析")
            print("       安装命令: pip install anthropic")
        except Exception as e:
            print(f"[ERROR] Claude分析失败: {e}")

    def stop(self):
        """停止接收器并显示统计信息"""
        duration = datetime.datetime.now() - self.stats['start_time']

        print("\n" + "=" * 60)
        print("统计信息:")
        print(f"  运行时长: {duration}")
        print(f"  接收数据包总数: {self.stats['total_packets']}")
        print(f"  接收字节总数: {self.stats['total_bytes']}")
        print(f"  平均包大小: {self.stats['total_bytes'] / self.stats['total_packets'] if self.stats['total_packets'] > 0 else 0:.2f} 字节")
        print(f"  日志文件: {self.log_file}")
        print("=" * 60)

        # 等待用户确认后退出程序（手动退出）
        try:
            input("\n按 Enter 键退出程序...")
        except (KeyboardInterrupt, EOFError):
            pass
        sys.exit(0)

def main():
    parser = argparse.ArgumentParser(description='ESP32 UDP接收器与Claude分析工具')
    parser.add_argument('-p', '--port', type=int, default=8888, help='UDP监听端口 (默认: 8888)')
    parser.add_argument('-i', '--ip', type=str, default='0.0.0.0', help='监听IP地址 (默认: 0.0.0.0)')
    parser.add_argument('-s', '--source', type=str, help='仅接收指定来源 IP 或 IP:PORT，例如 192.168.1.100 或 192.168.1.100:5000')
    parser.add_argument('--enable-claude', action='store_true', help='启用Claude AI分析')
    parser.add_argument('--api-key', type=str, help='Anthropic API密钥')

    args = parser.parse_args()

    # 更新配置
    CONFIG['listen_port'] = args.port
    CONFIG['listen_ip'] = args.ip
    CONFIG['enable_claude'] = args.enable_claude
    CONFIG['claude_api_key'] = args.api_key or os.environ.get('ANTHROPIC_API_KEY')

    # 处理来源过滤参数：优先使用命令行参数，否则在交互式模式下询问（可留空）
    src = args.source
    if not src:
        try:
            # 在双击运行或交互运行时提示用户输入（可直接回车跳过）
            if os.name == 'nt':
                inp = input("指定来源 (ip[:port], 留空接收所有): ").strip()
                if inp:
                    src = inp
        except (KeyboardInterrupt, EOFError):
            src = None

    if src:
        # 解析 ip[:port]
        parts = src.split(':')
        ip = parts[0] if parts[0] else None
        port = None
        if len(parts) > 1:
            try:
                port = int(parts[1])
            except ValueError:
                port = None
        CONFIG['source_filter'] = {'ip': ip, 'port': port}
    else:
        CONFIG['source_filter'] = None

    # 创建并启动接收器
    receiver = UDPReceiver(CONFIG)
    receiver.start()


if __name__ == '__main__':
    # 保证双击时工作目录为脚本所在目录（避免相对路径问题）
    try:
        os.chdir(os.path.dirname(os.path.abspath(__file__)))
    except Exception:
        pass

    try:
        main()
    except Exception as e:
        import traceback
        print(f"[FATAL] 未捕获异常: {e}")
        traceback.print_exc()
        # 在 Windows 双击运行时等待用户确认，避免窗口立即关闭
        if os.name == 'nt':
            try:
                input("\n按回车退出...")
            except (KeyboardInterrupt, EOFError):
                pass
        sys.exit(1)
    else:
        # 正常退出时在 Windows 下也等待用户确认（可选）
        if os.name == 'nt':
            try:
                input("\n按回车退出...")
            except (KeyboardInterrupt, EOFError):
                pass