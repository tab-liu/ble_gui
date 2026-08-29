#!/usr/bin/env python3
import sys
import os
import shutil
import hashlib
from datetime import datetime

def reversal32(b0, b1, b2, b3):
    """将4个字节按高低位反转"""
    return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3

def crc32_calc(icrc, data):
    """
    计算CRC32, 与crc.c中的calcu_crc32函数保持一致
    初始值: 由icrc参数指定
    多项式: 0x04C11DB7
    输入数据每4字节会进行高低字节反转
    输出结果不反转
    异或值: 0x00000000
    """
    crc = icrc
    length = len(data)
    pos = 0

    while length > 0:
        if length >= 4:
            # 大于4个字节，高低字节反转
            word = reversal32(data[pos], data[pos+1], data[pos+2], data[pos+3])
            length -= 4
            pos += 4
        else:
            # 小于4个字节，不足4字节补0
            word = 0
            for i in range(length):
                word |= data[pos + length - 1 - i] << (i * 8)
            length = 0

        crc ^= word
        for _ in range(32):
            if crc & 0x80000000:
                crc = ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF
            else:
                crc = (crc << 1) & 0xFFFFFFFF

    return crc

def add_crc32_to_bin(bin_file):
    # 读取原始bin文件
    with open(bin_file, 'rb') as f:
        data = f.read()
    
    original_size = len(data)
    print(f"Original file size: {original_size} bytes")
    
    # 步骤1: 计算添加CRC32(4字节)+16字节0xFF后是否会跨越1024字节边界
    additional_bytes = 4 + 16  # CRC32 + 16字节0xFF = 20字节
    current_position_in_block = original_size % 1024
    remaining_space_in_block = 1024 - current_position_in_block if current_position_in_block != 0 else 1024
    
    print(f"Current position in 1024-byte block: {current_position_in_block}")
    print(f"Remaining space in current block: {remaining_space_in_block}")
    print(f"Need to add: {additional_bytes} bytes (CRC32 + 16 bytes 0xFF)")
    
    # 步骤2: 如果剩余空间不足以容纳CRC32+16字节0xFF，则填充到1024字节边界
    if additional_bytes > remaining_space_in_block:
        padding_to_1024 = remaining_space_in_block
        data += bytes([0xFF] * padding_to_1024)
        print(f"Added {padding_to_1024} bytes of 0xFF padding to align to 1024-byte boundary")
    else:
        padding_to_1024 = 0
        print("No padding needed - CRC32 and final padding fit in current block")
    
    # 步骤3: 计算CRC32（基于可能填充后的数据）
    crc32 = crc32_calc(0xFFFFFFFF, data)
    print(f"Calculated CRC32: 0x{crc32:08X}")
    
    # 步骤4: 添加4字节CRC32（大端序）
    crc32_bytes = crc32.to_bytes(4, byteorder='big')
    data += crc32_bytes
    
    # 步骤5: 添加16字节的0xFF填充
    final_padding = bytes([0xFF] * 16)
    data += final_padding
    
    # 写入完整的文件
    with open(bin_file, 'wb') as f:
        f.write(data)
    
    final_size = len(data)
    print(f"Final file size: {final_size} bytes")
    print(f"Total added: {final_size - original_size} bytes")
    print(f"  - 1024-byte alignment padding: {padding_to_1024} bytes")
    print(f"  - CRC32: 4 bytes")
    print(f"  - Final padding: 16 bytes")
    
    # 计算整个文件的MD5
    md5_hash = hashlib.md5(data).hexdigest()
    print(f"File MD5: {md5_hash.upper()}")

def process_bin_file(original_bin):
    # 获取当前日期
    build_date = datetime.now().strftime("%Y%m%d")
    
    # 构建新的文件名
    # new_bin_name = f"{os.path.basename(original_bin).replace('.bin', '')}_CRC32_{build_date}.bin"
    new_bin_name = f"{os.path.basename(original_bin).replace('.bin', '')}_OTA_CRC32.bin"

    # 获取原始文件的目录
    bin_dir = os.path.dirname(original_bin)
    new_bin_path = os.path.join(bin_dir, new_bin_name)
    
    # 复制并重命名文件
    shutil.copy2(original_bin, new_bin_path)
    print(f"Copied {original_bin} to {new_bin_path}")
    
    # 添加CRC32到新文件
    add_crc32_to_bin(new_bin_path)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python add_crc32.py <bin_file>")
        sys.exit(1)
    
    bin_file = sys.argv[1]
    if not os.path.exists(bin_file):
        print(f"Error: File {bin_file} does not exist")
        sys.exit(1)
    
    process_bin_file(bin_file) 